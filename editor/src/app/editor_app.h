#ifndef SCENIC_EDITOR_APP_H
#define SCENIC_EDITOR_APP_H

#include <chrono>
#include <string>

#include "app/file_watch.h"
#include "app/preferences.h"
#include "document/session.h"
#include "testing/script.h"
#include "transport/live_requests.h"
#include "ui/assets_panel.h"
#include "ui/dialogs.h"
#include "ui/explorer_panel.h"
#include "ui/log_panel.h"
#include "ui/properties_panel.h"
#include "viewport/viewport.h"

struct EditorOptions {
  std::string project;
  std::string level;
  std::string script;
  std::string log;
};

class EditorApp {
 public:
  EditorApp(const Project& project, const EditorOptions& options);
  ~EditorApp();

  bool run();

 private:
  void frame();
  void pollFiles();
  void buildInterface();
  void finishScreenshot();
  void frameNewDocument();

  EditorSession session;
  Viewport viewport;
  LevelDialogs dialogs;
  ExplorerPanel explorer;
  PropertiesPanel properties;
  AssetsPanel assets;
  LogPanel log;
  LiveRequests liveRequests;
  EditorScript script;
  Preferences preferences;
  ImageWatcher imageWatcher;
  Interval diskPoll{std::chrono::milliseconds(500)};
  bool resetLayout = false;
  std::string framedDocument;
};

#endif  // !SCENIC_EDITOR_APP_H
