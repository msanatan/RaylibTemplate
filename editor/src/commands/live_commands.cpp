#include <algorithm>
#include <filesystem>

#include "commands/editor_log.h"
#include "document/session.h"
#include "levels/document/validation.h"
#include "levels/editing/operations.h"
#include "raylib.h"

namespace {
EditorView& RequireView(EditorSession& session) {
  if (session.view == nullptr) {
    throw CommandFailure{CommandExit::Invalid, "no_viewport",
                         "This editor session has no viewport"};
  }
  return *session.view;
}

void RequireOpen(const EditorSession& session) {
  if (!session.doc().isOpen()) {
    throw CommandFailure{CommandExit::Invalid, "no_document",
                         "No level is open"};
  }
}

void ResolveUnsavedChanges(EditorSession& session, const LevelJson& args) {
  session.requireNoGesture();
  if (!session.doc().isDirty()) {
    return;
  }
  const std::string choice = args.value("unsaved", "");
  if (choice != "save" && choice != "discard") {
    throw CommandFailure{CommandExit::Conflict, "unsaved_changes",
                         "The open level has unsaved changes. Pass --unsaved "
                         "save or --unsaved discard"};
  }
  // The caller is deciding the fate of a specific state, so it must name it
  if (!args.contains("document") || !args.contains("expect")) {
    throw CommandFailure{CommandExit::Invalid, "missing_argument",
                         "--unsaved needs --document and --expect to identify "
                         "the unsaved state"};
  }
  RequireRevision(session, args);
  if (choice == "save") {
    session.save();
  }
}

LevelJson AssetWarnings(const EditorSession& session) {
  LevelJson warnings = LevelJson::array();
  for (const LevelDiagnostic& warning : ValidateLevelAssets(
           session.doc().level(), session.project.assetsDirectory)) {
    warnings.push_back(DiagnosticToJson(warning));
  }
  return warnings;
}

LevelJson NewLevel(EditorSession& session, const LevelJson& args) {
  const std::string path =
      session.project.resolve(args["path"].get<std::string>());
  if (std::filesystem::exists(path)) {
    throw CommandFailure{
        CommandExit::Io, "file_exists",
        "\"" + path + "\" already exists; `level new` never overwrites"};
  }
  ResolveUnsavedChanges(session, args);
  session.newLevel(path);
  return {{"changed", true}, {"completed", "document_opened"}};
}

LevelJson OpenLevel(EditorSession& session, const LevelJson& args) {
  const std::string path =
      session.project.resolve(args["path"].get<std::string>());
  EditorDocument candidate = session.prepareOpenLevel(path);
  ResolveUnsavedChanges(session, args);
  session.activateLevel(std::move(candidate));
  return {{"changed", true},
          {"completed", "document_opened"},
          {"warnings", AssetWarnings(session)}};
}

LevelJson Save(EditorSession& session) {
  session.save();
  return {{"completed", "save"}, {"saved", true}};
}

LevelJson UndoOrRedo(EditorSession& session, bool undo) {
  session.requireNoGesture();
  const Level before = session.doc().level();
  if (!(undo ? session.undo() : session.redo())) {
    return {{"completed", nullptr}};
  }
  std::vector<std::string> affected;
  const LevelJson changes =
      DescribeLevelChanges(before, session.doc().level(), affected);
  return {{"changed", true},
          {"affected_ids", affected},
          {"change_count", changes.size()},
          {"completed", "live_transaction"}};
}

std::vector<std::string> ExistingIds(const EditorSession& session,
                                     const LevelJson& args) {
  std::vector<std::string> ids;
  for (const LevelJson& id : args["ids"]) {
    if (session.doc().index().find(id.get<std::string>()) == nullptr) {
      throw CommandFailure{CommandExit::Invalid, "unknown_object",
                           "Object " + id.dump() + " does not exist"};
    }
    ids.push_back(id.get<std::string>());
  }
  return ids;
}

LevelJson Select(EditorSession& session, const LevelJson& args) {
  RequireOpen(session);
  session.selection.set(args.contains("ids") ? ExistingIds(session, args)
                                             : std::vector<std::string>());
  return {{"selection", session.selection.ids()}};
}

LevelJson Frame(EditorSession& session, const LevelJson& args) {
  RequireOpen(session);
  EditorView& view = RequireView(session);
  const std::vector<std::string> ids = args.contains("ids")
                                           ? ExistingIds(session, args)
                                           : session.selection.ids();
  view.frameObjects(ids);
  return {{"framed", ids}, {"view", view.viewState()}};
}

LevelJson CameraPreview(EditorSession& session, const LevelJson& args) {
  RequireOpen(session);
  EditorView& view = RequireView(session);
  const std::string mode = args["mode"].get<std::string>();
  if (mode != "enter" && mode != "leave") {
    throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                         "Use `camera preview enter` or `camera preview "
                         "leave`"};
  }
  std::string error;
  if (!view.setCameraPreview(mode == "enter", error)) {
    throw CommandFailure{CommandExit::Invalid, "no_level_camera", error};
  }
  return {{"view", view.viewState()}};
}

LevelJson Screenshot(EditorSession& session, const LevelJson& args) {
  EditorView& view = RequireView(session);
  const std::string area = args.value("area", "viewport");
  if (area != "viewport" && area != "editor") {
    throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                         "--area must be viewport or editor"};
  }
  const std::string path =
      session.project.resolve(args["path"].get<std::string>());
  view.requestScreenshot(path, area == "viewport");
  return {{"path", path},
          {"area", area},
          {"camera", view.isPreviewingCamera() ? "level_camera" : "navigation"},
          {"rendered_revision", session.doc().isOpen()
                                    ? LevelJson(session.doc().revision())
                                    : LevelJson()},
          {"completed", "rendered_capture"}};
}

int SeverityLevel(const std::string& name) {
  for (const int level :
       {LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_FATAL}) {
    if (name == LogSeverityName(level)) {
      return level;
    }
  }
  throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                       "--severity must be trace, debug, info, warning, error, "
                       "or fatal"};
}

LevelJson Logs(EditorSession& session, const LevelJson& args) {
  const int minimum = args.contains("severity")
                          ? SeverityLevel(args["severity"].get<std::string>())
                          : LOG_TRACE;
  const std::string subsystem = args.value("subsystem", "");
  const std::string request = args.value("request", "");

  // Log cursors hold a sequence number, not an offset, so they stay valid as
  // new records arrive
  LevelJson filters = args;
  filters.erase("cursor");
  filters.erase("limit");
  const std::string scope = "logs|" + session.sessionId + "|" + filters.dump();
  const CommandPage page = ReadPage(args, scope);
  const bool continuing = args.contains("cursor");

  const uint64_t oldest = OldestLogSequence();
  uint64_t dropped = 0;
  if (continuing && page.offset + 1 < oldest) {
    dropped = oldest - page.offset - 1;
  }

  std::vector<LogRecord> matching;
  for (const LogRecord& record :
       LogRecordsAfter(continuing ? page.offset : 0)) {
    if (record.level >= minimum &&
        (subsystem.empty() || record.subsystem == subsystem) &&
        (request.empty() || record.requestId == request)) {
      matching.push_back(record);
    }
  }

  size_t first = 0;
  size_t count = std::min(matching.size(), page.limit);
  if (!continuing) {
    first = matching.size() - count;
  }
  const bool hasMore = continuing && matching.size() > count;
  const uint64_t resumeAfter =
      hasMore ? matching[first + count - 1].sequence : NewestLogSequence();

  LevelJson records = LevelJson::array();
  for (size_t i = first; i < first + count; i++) {
    const LogRecord& record = matching[i];
    LevelJson json = {{"sequence", record.sequence},
                      {"severity", LogSeverityName(record.level)},
                      {"subsystem", record.subsystem},
                      {"message", record.message}};
    if (record.message.size() > maximumValueLength) {
      json["message"] = record.message.substr(0, maximumValueLength);
      json["truncated"] = true;
    }
    if (!record.requestId.empty()) {
      json["request_id"] = record.requestId;
    }
    records.push_back(json);
  }

  return {{"records", records},
          {"has_more", hasMore},
          {"next_cursor", HashText(scope) + ":" + std::to_string(resumeAfter)},
          {"cursor_expired", dropped > 0},
          {"dropped", dropped}};
}

std::function<LevelJson(CommandTarget&, const LevelJson&)> Bind(
    const std::function<LevelJson(EditorSession&, const LevelJson&)>& run) {
  return [run](CommandTarget& target, const LevelJson& args) {
    return run(dynamic_cast<EditorSession&>(target), args);
  };
}
}  // namespace

void RegisterLiveCommands(CommandRegistry& registry) {
  const std::vector<CommandParam> switchParams = {
      {"path", "string", true, "Level path, relative to the project root"},
      {"unsaved", "string", false,
       "save or discard: what to do with the open level's unsaved changes. "
       "Needs --document and --expect"},
      {"expect", "string", false, "The open document's revision"},
  };
  registry.add({
      .name = "level new",
      .description =
          "Start a valid empty level, unsaved, at a path that does not exist.",
      .supportsEditor = true,
      .hasSideEffects = true,
      .params = switchParams,
      .resultDescription = "completed: document_opened.",
      .run = Bind(NewLevel),
  });
  registry.add({
      .name = "level open",
      .description =
          "Validate and open an existing level. Missing assets are warnings.",
      .supportsEditor = true,
      .hasSideEffects = true,
      .params = switchParams,
      .resultDescription = "completed: document_opened.",
      .run = Bind(OpenLevel),
  });
  registry.add({
      .name = "save",
      .description =
          "Validate the open level and write it atomically in canonical form. "
          "Fails if the file changed on disk.",
      .supportsEditor = true,
      .mutates = true,
      .hasSideEffects = true,
      .resultDescription = "saved: true; completed: save.",
      .run = Bind([](EditorSession& s, const LevelJson&) { return Save(s); }),
  });
  for (const bool undo : {true, false}) {
    registry.add({
        .name = undo ? "undo" : "redo",
        .description = undo ? "Undo the last document transaction."
                            : "Redo the last undone transaction.",
        .supportsEditor = true,
        .mutates = true,
        .hasSideEffects = true,
        .resultDescription = "change_count; completed: live_transaction, or "
                             "null when there was nothing to undo or redo.",
        .run = Bind([undo](EditorSession& s, const LevelJson&) {
          return UndoOrRedo(s, undo);
        }),
    });
  }
  registry.add({
      .name = "select",
      .description = "Select exact object IDs in the Explorer and viewport.",
      .supportsEditor = true,
      .hasSideEffects = true,
      .params = {{"ids", "strings", false,
                  "Object IDs; omit to clear the selection"}},
      .resultDescription = "selection: selected object IDs.",
      .run = Bind(Select),
  });
  registry.add({
      .name = "frame",
      .description =
          "Point the editor camera at objects without changing the level or "
          "the selection. Leaves the level camera preview.",
      .supportsEditor = true,
      .hasSideEffects = true,
      .params = {{"ids", "strings", false,
                  "Object IDs; defaults to the selection, then everything"}},
      .resultDescription =
          "framed: object IDs; view: the resulting editor camera state.",
      .run = Bind(Frame),
  });
  registry.add({
      .name = "camera preview",
      .description =
          "Look through the designated level camera, or return to the editor "
          "camera. Authored data is unchanged.",
      .supportsEditor = true,
      .hasSideEffects = true,
      .params = {{"mode", "string", true, "enter or leave"}},
      .resultDescription = "view: the resulting editor camera state.",
      .positional = "mode",
      .run = Bind(CameraPreview),
  });
  registry.add({
      .name = "screenshot",
      .description =
          "Save a PNG of the viewport or the whole editor after the next "
          "completed frame.",
      .supportsEditor = true,
      .hasSideEffects = true,
      .params = {{"path", "string", true,
                  "PNG path, relative to the project root"},
                 {"area", "string", false, "viewport (default) or editor"}},
      .resultDescription =
          "path: saved PNG; camera: navigation or level_camera; "
          "rendered_revision; completed: rendered_capture.",
      .run = Bind(Screenshot),
  });
  registry.add({
      .name = "logs",
      .description =
          "Read editor log records. Without a cursor, the most recent page; "
          "with next_cursor, the records that followed.",
      .supportsEditor = true,
      .params =
          {{"severity", "string", false,
            "Minimum severity: trace, debug, info, warning, error, or fatal"},
           {"subsystem", "string", false, "Such as SCENIC or LEVEL"},
           {"request", "string", false, "Only records from this request ID"},
           {"limit", "integer", false,
            "Records per page, 1 to 200. Defaults to 50."},
           {"cursor", "string", false,
            "next_cursor from an earlier call with the same filters"}},
      .resultDescription =
          "records: sequence, severity, subsystem, message, and request ID. "
          "next_cursor resumes after this page; cursor_expired and dropped "
          "report discarded records.",
      .run = Bind(Logs),
  });
  registry.add({
      .name = "state",
      .description =
          "Dump editor state: document, selection, history, camera, and the "
          "latest operation and reload results.",
      .supportsEditor = true,
      .resultDescription = "state: document, selection, history, camera, and "
                           "latest operation and reload results.",
      .run = Bind([](EditorSession& s, const LevelJson&) {
        return LevelJson{{"state", s.state()}};
      }),
  });
}
