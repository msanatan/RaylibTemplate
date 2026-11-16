#ifndef SCENIC_DOCUMENT_H
#define SCENIC_DOCUMENT_H

#include <optional>
#include <string>

#include "document/history.h"
#include "levels/document/diagnostics.h"
#include "levels/document/level.h"
#include "levels/document/level_index.h"
#include "levels/editing/operations.h"

class EditorDocument {
 public:
  bool isOpen() const { return open; }
  const std::string& id() const { return documentId; }
  const std::string& path() const { return file; }
  const Level& level() const { return current; }
  const LevelIndex& index() const;
  std::string revision() const;
  bool isDirty() const;
  const History& history() const { return steps; }

  void createNew(const std::string& path);
  bool openFile(const std::string& path, LevelDiagnostics& errors);
  void save();

  LevelEditResult apply(const LevelJson& request, const std::string& label);
  void commit(const Level& next, const std::string& label);
  bool undo();
  bool redo();

  // A continuous edit such as a drag. Only the owner that began a gesture
  // can preview, finish, or cancel it, so two controls cannot end each
  // other's edits.
  bool beginGesture(const std::string& owner);
  bool isGestureActive() const { return !gestureOwner.empty(); }
  bool ownsGesture(const std::string& owner) const;
  void previewGesture(const std::string& owner, const Level& preview);
  bool finishGesture(const std::string& owner, const std::string& label);
  void cancelGesture(const std::string& owner);

  enum class DiskChange { None, Reloaded, Conflict, Invalid };
  DiskChange pollDisk(LevelDiagnostics& errors);
  bool hasDiskConflict() const { return diskConflict; }
  bool reloadFromDisk(LevelDiagnostics& errors);
  void keepLocalChanges();

 private:
  // A copied document must not reuse an index that points into the original
  struct IndexCache {
    IndexCache() = default;
    IndexCache(const IndexCache&) {}
    IndexCache& operator=(const IndexCache&) {
      index.reset();
      return *this;
    }
    mutable std::optional<LevelIndex> index;
  };

  void setLevel(const Level& level);
  void replace(const Level& level);
  void adopt(const std::string& path, const Level& level,
             const std::string& onDiskRevision);
  std::string readDiskRevision(std::string& text) const;

  bool open = false;
  std::string documentId;
  std::string file;
  Level current;
  IndexCache cache;
  Level saved;
  bool onDisk = false;
  std::string diskRevision;
  std::string reportedDiskRevision;
  bool diskConflict = false;
  unsigned long long revisionCounter = 0;
  History steps;
  std::string gestureOwner;
  Level gestureStart;
};

#endif  // !SCENIC_DOCUMENT_H
