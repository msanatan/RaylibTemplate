#ifndef SCENIC_UI_CONTEXT_H
#define SCENIC_UI_CONTEXT_H

class EditorSession;
class LevelDialogs;
class Viewport;

struct UiContext {
  EditorSession& session;
  Viewport& viewport;
  LevelDialogs& dialogs;
};

#endif  // !SCENIC_UI_CONTEXT_H
