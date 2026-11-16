#include "document/session.h"

#include <filesystem>

#include "commands/editor_log.h"
#include "levels/editing/file_target.h"
#include "raylib.h"

namespace {
const size_t rememberedRequests = 256;

// Braces around a single JSON value copy it instead of making an array, so
// callers may pass either one operation or an array
LevelJson Batch(const LevelJson& operations) {
  LevelJson request = {{"operations", LevelJson::array()}};
  if (operations.is_array()) {
    request["operations"] = operations;
  } else {
    request["operations"].push_back(operations);
  }
  return request;
}

bool HasSideEffects(const CommandRegistry& registry,
                    const CommandRequest& request) {
  std::string name;
  for (const std::string& word : request.words) {
    name += (name.empty() ? "" : " ") + word;
    const Command* command = registry.find(name, "editor");
    if (command != nullptr && command->hasSideEffects) {
      return true;
    }
  }
  return false;
}
}  // namespace

EditorSession::EditorSession(Project project) : project(std::move(project)) {
  RegisterDocumentCommands(registry);
  RegisterLiveCommands(registry);
  RegisterFileCommands(registry);
}

void EditorSession::documentChanged(const std::string& what) {
  selection.dropMissing(openDocument.index());
  lastOperation = what;
  TraceLog(LOG_INFO, "SCENIC: %s (revision %s)", what.c_str(),
           openDocument.revision().c_str());
}

bool EditorSession::logFailure(const std::string& action,
                               const CommandFailure& failure) {
  lastOperation = action + " failed: " + failure.message;
  TraceLog(LOG_ERROR, "SCENIC: %s", lastOperation.c_str());
  return false;
}

void EditorSession::requireNoGesture() const {
  if (openDocument.isGestureActive()) {
    throw CommandFailure{CommandExit::Conflict, "gesture_in_progress",
                         "The user is in the middle of an edit in the editor. "
                         "Try again once it finishes"};
  }
}

bool EditorSession::edit(const LevelJson& operations,
                         const std::string& label) {
  if (!openDocument.isOpen() || openDocument.isGestureActive()) {
    return false;
  }
  const LevelEditResult result = openDocument.apply(Batch(operations), label);
  if (!result.ok) {
    lastOperation = label + ": " + DescribeDiagnostic(result.errors.front());
    TraceLog(LOG_WARNING, "SCENIC: %s failed: %s", label.c_str(),
             DescribeDiagnostic(result.errors.front()).c_str());
    return false;
  }
  if (result.changed) {
    documentChanged(label);
  }
  return true;
}

void EditorSession::beginGesture(const std::string& owner) {
  openDocument.beginGesture(owner);
}

bool EditorSession::previewEdit(const std::string& owner,
                                const LevelJson& operations) {
  if (!openDocument.ownsGesture(owner)) {
    return false;
  }
  const LevelEditResult result =
      ApplyLevelOperations(openDocument.level(), Batch(operations));
  if (result.ok) {
    openDocument.previewGesture(owner, result.level);
  }
  return result.ok;
}

void EditorSession::finishGesture(const std::string& owner,
                                  const std::string& label) {
  if (openDocument.finishGesture(owner, label)) {
    documentChanged(label);
  }
}

void EditorSession::cancelGesture(const std::string& owner) {
  openDocument.cancelGesture(owner);
}

bool EditorSession::undo() {
  if (openDocument.isGestureActive() || !openDocument.undo()) {
    return false;
  }
  documentChanged("Undo");
  return true;
}

bool EditorSession::redo() {
  if (openDocument.isGestureActive() || !openDocument.redo()) {
    return false;
  }
  documentChanged("Redo");
  return true;
}

void EditorSession::save() {
  if (!openDocument.isOpen()) {
    throw CommandFailure{CommandExit::Invalid, "no_document",
                         "No level is open"};
  }
  requireNoGesture();
  openDocument.save();
  lastOperation = "Saved";
  TraceLog(LOG_INFO, "SCENIC: Saved [%s]", openDocument.path().c_str());
}

void EditorSession::newLevel(const std::string& path) {
  requireNoGesture();
  if (std::filesystem::exists(path)) {
    throw CommandFailure{
        CommandExit::Io, "file_exists",
        "\"" + path + "\" already exists; a new level never overwrites"};
  }
  openDocument.createNew(path);
  selection.clear();
  documentChanged("New level " + path);
}

void EditorSession::openLevel(const std::string& path) {
  activateLevel(prepareOpenLevel(path));
}

EditorDocument EditorSession::prepareOpenLevel(const std::string& path) const {
  requireNoGesture();
  EditorDocument candidate;
  LevelDiagnostics errors;
  if (!candidate.openFile(path, errors)) {
    const bool missing = errors.front().code == "file_not_found";
    throw CommandFailure{missing ? CommandExit::Io : CommandExit::Invalid,
                         missing ? "file_not_found" : "invalid_document",
                         "\"" + path + "\" was not opened: " +
                             DescribeDiagnostic(errors.front()),
                         errors};
  }
  return candidate;
}

void EditorSession::activateLevel(EditorDocument candidate) {
  requireNoGesture();
  openDocument = std::move(candidate);
  selection.clear();
  documentChanged("Opened " + openDocument.path());
}

bool EditorSession::trySave() {
  try {
    save();
    return true;
  } catch (const CommandFailure& failure) {
    return logFailure("Save", failure);
  }
}

bool EditorSession::tryNewLevel(const std::string& path) {
  try {
    newLevel(path);
    return true;
  } catch (const CommandFailure& failure) {
    return logFailure("New level", failure);
  }
}

bool EditorSession::tryOpenLevel(const std::string& path) {
  try {
    openLevel(path);
    return true;
  } catch (const CommandFailure& failure) {
    return logFailure("Open", failure);
  }
}

void EditorSession::pollDisk() {
  LevelDiagnostics errors;
  switch (openDocument.pollDisk(errors)) {
    case EditorDocument::DiskChange::None:
      break;
    case EditorDocument::DiskChange::Reloaded:
      lastReload = "Reloaded the level from disk";
      documentChanged(lastReload);
      break;
    case EditorDocument::DiskChange::Conflict:
      lastReload = "The level changed on disk; unsaved edits kept";
      TraceLog(LOG_WARNING,
               "SCENIC: [%s] changed on disk while it has unsaved edits",
               openDocument.path().c_str());
      break;
    case EditorDocument::DiskChange::Invalid:
      lastReload = "Ignored an invalid change on disk: " +
                   DescribeDiagnostic(errors.front());
      for (const LevelDiagnostic& error : errors) {
        TraceLog(LOG_ERROR,
                 "LEVEL: [%s] changed on disk but is invalid, so the open "
                 "level was kept: %s",
                 openDocument.path().c_str(),
                 DescribeDiagnostic(error).c_str());
      }
      break;
  }
}

void EditorSession::reloadFromDisk() {
  LevelDiagnostics errors;
  if (!openDocument.isGestureActive() && openDocument.reloadFromDisk(errors)) {
    lastReload = "Reloaded the level from disk";
    documentChanged(lastReload);
  }
}

void EditorSession::keepLocalChanges() { openDocument.keepLocalChanges(); }

LevelJson EditorSession::dispatch(const CommandRequest& request) {
  const bool remember =
      !request.requestId.empty() && HasSideEffects(registry, request);
  const std::string fingerprint =
      HashText(LevelJson(request.words).dump() + request.args.dump());
  if (remember) {
    const auto completed = completedRequests.find(request.requestId);
    if (completed != completedRequests.end()) {
      if (completed->second.fingerprint != fingerprint) {
        LevelJson result =
            FailureResult({CommandExit::Invalid, "request_id_reused",
                           "Request ID \"" + request.requestId +
                               "\" was already used for a different command"});
        result["request_id"] = request.requestId;
        return result;
      }
      LevelJson result = completed->second.result;
      result["deduplicated"] = true;
      return result;
    }
  }

  SetLogRequestId(request.requestId);
  LevelJson result = registry.dispatch(*this, request);
  SetLogRequestId("");

  if (remember) {
    completedRequests[request.requestId] = {fingerprint, result};
    completedOrder.push_back(request.requestId);
    if (completedOrder.size() > rememberedRequests) {
      completedRequests.erase(completedOrder.front());
      completedOrder.erase(completedOrder.begin());
    }
  }
  return result;
}

LevelJson EditorSession::state() const {
  LevelJson state = {
      {"session", sessionId},
      {"project", project.root},
      {"document", nullptr},
      {"selection", selection.ids()},
      {"last_operation", lastOperation},
      {"last_reload", lastReload},
      {"view", view != nullptr ? view->viewState() : LevelJson()},
  };
  if (!openDocument.isOpen()) {
    return state;
  }
  if (const LevelObject* primary =
          openDocument.index().find(selection.primary())) {
    const LevelVec3& position = primary->transform.position;
    state["primary_position"] = {position.x, position.y, position.z};
  }
  state["document"] = {
      {"id", openDocument.id()},
      {"path", openDocument.path()},
      {"version", openDocument.level().version},
      {"revision", openDocument.revision()},
      {"dirty", openDocument.isDirty()},
      {"object_count", openDocument.level().objects.size()},
      {"undo_steps", openDocument.history().undoCount()},
      {"redo_steps", openDocument.history().redoCount()},
      {"disk_conflict", openDocument.hasDiskConflict()},
  };
  return state;
}

DocumentState EditorSession::document() const {
  DocumentState state;
  if (openDocument.isOpen()) {
    state.open = true;
    state.id = openDocument.id();
    state.path = openDocument.path();
    state.revision = openDocument.revision();
    state.dirty = openDocument.isDirty();
    state.level = &openDocument.level();
  }
  return state;
}

std::string EditorSession::commit(const Level& level,
                                  const std::string& label) {
  requireNoGesture();
  openDocument.commit(level, label);
  documentChanged(label);
  return "live_transaction";
}
