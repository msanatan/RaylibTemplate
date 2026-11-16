#include "document/document.h"

#include "levels/document/serialization.h"
#include "levels/document/validation.h"
#include "levels/editing/file_target.h"

const LevelIndex& EditorDocument::index() const {
  if (!cache.index.has_value()) {
    cache.index.emplace(current);
  }
  return *cache.index;
}

std::string EditorDocument::revision() const {
  return "live:" + std::to_string(revisionCounter);
}

bool EditorDocument::isDirty() const {
  return open && (!onDisk || !(current == saved));
}

void EditorDocument::setLevel(const Level& level) {
  current = level;
  cache.index.reset();
}

void EditorDocument::replace(const Level& level) {
  setLevel(level);
  revisionCounter++;
}

void EditorDocument::adopt(const std::string& path, const Level& level,
                           const std::string& onDiskRevision) {
  open = true;
  file = path;
  onDisk = !onDiskRevision.empty();
  saved = level;
  diskRevision = onDiskRevision;
  reportedDiskRevision = onDiskRevision;
  diskConflict = false;
  gestureOwner.clear();
  // Undoing across a replacement would resurrect content it discarded
  steps.clear();
  replace(level);
}

void EditorDocument::createNew(const std::string& path) {
  documentId = NewLevelId();
  adopt(path, Level(), "");
}

bool EditorDocument::openFile(const std::string& path,
                              LevelDiagnostics& errors) {
  const std::optional<std::string> text = ReadTextFile(path);
  if (!text.has_value()) {
    LevelDiagnostic missing;
    missing.code = "file_not_found";
    missing.message = "\"" + path + "\" does not exist";
    errors.push_back(missing);
    return false;
  }
  const std::optional<Level> level = ParseValidLevel(*text, errors);
  if (!level.has_value()) {
    return false;
  }
  documentId = NewLevelId();
  adopt(path, *level, FileRevision(*text));
  return true;
}

void EditorDocument::save() {
  const LevelDiagnostics errors = ValidateLevel(current);
  if (!errors.empty()) {
    throw CommandFailure{CommandExit::Invalid, "invalid_document",
                         "The level is invalid and was not saved: " +
                             DescribeDiagnostic(errors.front()),
                         errors};
  }
  const std::string text = SerializeLevel(current);
  WriteLevelFile(file, text, diskRevision);
  onDisk = true;
  saved = current;
  diskRevision = FileRevision(text);
  reportedDiskRevision = diskRevision;
  diskConflict = false;
}

LevelEditResult EditorDocument::apply(const LevelJson& request,
                                      const std::string& label) {
  LevelEditResult result = ApplyLevelOperations(current, request);
  if (result.ok && result.changed) {
    commit(result.level, label);
  }
  return result;
}

void EditorDocument::commit(const Level& next, const std::string& label) {
  steps.record(current, label);
  replace(next);
}

bool EditorDocument::undo() {
  const std::optional<Level> previous = steps.undo(current);
  if (previous.has_value()) {
    replace(*previous);
  }
  return previous.has_value();
}

bool EditorDocument::redo() {
  const std::optional<Level> next = steps.redo(current);
  if (next.has_value()) {
    replace(*next);
  }
  return next.has_value();
}

bool EditorDocument::beginGesture(const std::string& owner) {
  if (!open || (isGestureActive() && gestureOwner != owner)) {
    return false;
  }
  if (!isGestureActive()) {
    gestureOwner = owner;
    gestureStart = current;
  }
  return true;
}

bool EditorDocument::ownsGesture(const std::string& owner) const {
  return isGestureActive() && gestureOwner == owner;
}

void EditorDocument::previewGesture(const std::string& owner,
                                    const Level& preview) {
  if (ownsGesture(owner)) {
    setLevel(preview);
  }
}

bool EditorDocument::finishGesture(const std::string& owner,
                                   const std::string& label) {
  if (!ownsGesture(owner)) {
    return false;
  }
  gestureOwner.clear();
  if (current == gestureStart) {
    return false;
  }
  steps.record(gestureStart, label);
  revisionCounter++;
  return true;
}

void EditorDocument::cancelGesture(const std::string& owner) {
  if (ownsGesture(owner)) {
    gestureOwner.clear();
    setLevel(gestureStart);
  }
}

std::string EditorDocument::readDiskRevision(std::string& text) const {
  const std::optional<std::string> content = ReadTextFile(file);
  text = content.value_or("");
  return content.has_value() ? FileRevision(*content) : "";
}

EditorDocument::DiskChange EditorDocument::pollDisk(LevelDiagnostics& errors) {
  if (!open || isGestureActive()) {
    return DiskChange::None;
  }
  std::string text;
  const std::string onDiskNow = readDiskRevision(text);
  if (onDiskNow.empty() || onDiskNow == diskRevision ||
      onDiskNow == reportedDiskRevision) {
    return DiskChange::None;
  }
  reportedDiskRevision = onDiskNow;

  if (!ParseValidLevel(text, errors).has_value()) {
    return DiskChange::Invalid;
  }
  if (isDirty()) {
    diskConflict = true;
    return DiskChange::Conflict;
  }
  return reloadFromDisk(errors) ? DiskChange::Reloaded : DiskChange::Invalid;
}

bool EditorDocument::reloadFromDisk(LevelDiagnostics& errors) {
  std::string text;
  const std::string onDiskNow = readDiskRevision(text);
  const std::optional<Level> level = ParseValidLevel(text, errors);
  if (onDiskNow.empty() || !level.has_value()) {
    return false;
  }
  adopt(file, *level, onDiskNow);
  return true;
}

void EditorDocument::keepLocalChanges() {
  std::string text;
  diskRevision = readDiskRevision(text);
  reportedDiskRevision = diskRevision;
  diskConflict = false;
}
