#ifndef SCENIC_SESSION_H
#define SCENIC_SESSION_H

#include <map>
#include <string>
#include <vector>

#include "document/document.h"
#include "document/project.h"
#include "document/selection.h"
#include "levels/editing/commands.h"

class EditorView {
 public:
  virtual ~EditorView() = default;
  virtual void frameObjects(const std::vector<std::string>& ids) = 0;
  virtual bool setCameraPreview(bool preview, std::string& error) = 0;
  virtual bool isPreviewingCamera() const = 0;
  virtual void requestScreenshot(const std::string& path,
                                 bool viewportOnly) = 0;
  virtual LevelJson viewState() const = 0;
};

// One running editor without any window. Every change to the document goes
// through this class, whether it comes from a panel, a shortcut, or a live
// command, so the rules are applied once: no edit interrupts a gesture,
// selection never points at a missing object, and each change is logged.
// Panels read `doc()` and call these methods; they never mutate the document.
class EditorSession : public CommandTarget {
 public:
  explicit EditorSession(Project project);

  const EditorDocument& doc() const { return openDocument; }

  bool edit(const LevelJson& operations, const std::string& label);

  void beginGesture(const std::string& owner);
  bool previewEdit(const std::string& owner, const LevelJson& operations);
  void finishGesture(const std::string& owner, const std::string& label);
  void cancelGesture(const std::string& owner);

  bool undo();
  bool redo();

  void save();
  void newLevel(const std::string& path);
  void openLevel(const std::string& path);
  EditorDocument prepareOpenLevel(const std::string& path) const;
  void activateLevel(EditorDocument candidate);
  // The same, logging the failure instead
  bool trySave();
  bool tryNewLevel(const std::string& path);
  bool tryOpenLevel(const std::string& path);

  void pollDisk();
  void reloadFromDisk();
  void keepLocalChanges();

  LevelJson dispatch(const CommandRequest& request);
  void requireNoGesture() const;
  LevelJson state() const;

  const char* kind() const override { return "editor"; }
  std::string session() const override { return sessionId; }
  DocumentState document() const override;
  std::string assetsDirectory() const override {
    return project.assetsDirectory;
  }
  std::string commit(const Level& level, const std::string& label) override;

  Project project;
  Selection selection;
  EditorView* view = nullptr;
  std::string sessionId;
  CommandRegistry registry;
  std::string lastOperation;
  std::string lastReload;

 private:
  struct Completed {
    std::string fingerprint;
    LevelJson result;
  };

  void documentChanged(const std::string& what);
  bool logFailure(const std::string& action, const CommandFailure& failure);

  EditorDocument openDocument;
  std::map<std::string, Completed> completedRequests;
  std::vector<std::string> completedOrder;
};

void RegisterLiveCommands(CommandRegistry& registry);

#endif  // !SCENIC_SESSION_H
