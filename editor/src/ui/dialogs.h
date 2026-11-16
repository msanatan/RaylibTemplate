#ifndef SCENIC_DIALOGS_H
#define SCENIC_DIALOGS_H

#include <string>

class EditorSession;

class LevelDialogs {
 public:
  void promptNewLevel();
  void promptOpenLevel();
  void requestOpen(EditorSession& session, const std::string& path);
  void requestNew(EditorSession& session, const std::string& path);
  void requestQuit(EditorSession& session);
  bool hasQuit() const { return quit; }

  void draw(EditorSession& session);

 private:
  enum class Action { None, Open, New, Quit };

  void request(EditorSession& session, Action action, const std::string& path);
  void perform(EditorSession& session);
  void drawPathPrompt(EditorSession& session);
  void drawUnsavedPrompt(EditorSession& session);
  void drawDiskConflictPrompt(EditorSession& session);

  Action pathPrompt = Action::None;
  std::string pathInput;
  Action pending = Action::None;
  std::string pendingPath;
  bool quit = false;
};

#endif  // !SCENIC_DIALOGS_H
