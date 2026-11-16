#include "app/editor_app.h"

#include <filesystem>

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"
#include "ui/layout.h"
#include "ui/menu_bar.h"
#include "ui/script_targets.h"
#include "ui/shortcuts.h"
#include "ui/toolbar.h"
#include "viewport/gizmo_library.h"

namespace {
const float scriptedFrameTime = 1.0f / 60.0f;

// Scripted runs always start from the default layout so their clicks land in
// the same place, and never overwrite the user's own
bool ConfigureLayoutFile(const Project& project, bool scripted) {
  static std::string layoutFile;
  layoutFile =
      (std::filesystem::path(project.preferencesDirectory) / "layout.ini")
          .string();
  ImGuiIO& io = ImGui::GetIO();
  if (scripted) {
    io.IniFilename = nullptr;
    return true;
  }
  std::error_code error;
  std::filesystem::create_directories(project.preferencesDirectory, error);
  io.IniFilename = layoutFile.c_str();
  return !std::filesystem::exists(layoutFile);
}
}  // namespace

EditorApp::EditorApp(const Project& project, const EditorOptions& options)
    : session(project),
      viewport(session),
      preferences(LoadPreferences(project.preferencesDirectory)),
      imageWatcher(project.assetsDirectory) {
  session.view = &viewport;
  ApplyTheme(preferences.darkTheme);

  const bool scripted = !options.script.empty();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  resetLayout = ConfigureLayoutFile(project, scripted);

  std::string error;
  if (liveRequests.start(session, error)) {
    TraceLog(LOG_INFO, "SCENIC: Session [%s] for project [%s]",
             session.sessionId.c_str(), project.root.c_str());
  } else {
    TraceLog(LOG_WARNING, "SCENIC: Live commands are unavailable: %s",
             error.c_str());
  }

  const std::string level = options.level.empty()
                                ? project.defaultLevel
                                : project.resolve(options.level);
  if (!level.empty() && std::filesystem::exists(level)) {
    session.tryOpenLevel(level);
  } else if (!level.empty()) {
    session.tryNewLevel(level);
  }

  if (scripted && !script.load(options.script)) {
    script.fail("the script could not be loaded");
  }
}

EditorApp::~EditorApp() { liveRequests.stop(); }

bool EditorApp::run() {
  SetTargetFPS(script.isActive() ? 0 : 60);
  while (!dialogs.hasQuit() && !script.isFinished()) {
    if (WindowShouldClose()) {
      if (script.isActive()) {
        break;
      }
      dialogs.requestQuit(session);
    }
    frame();
  }
  TraceLog(LOG_INFO, "SCENIC: Closing");
  return !script.hasFailed();
}

void EditorApp::frame() {
  pollFiles();
  liveRequests.process(session);
  script.beginFrame(session, viewport);
  frameNewDocument();

  BeginDrawing();
  ClearBackground(BLACK);
  if (script.isActive()) {
    rlImGuiBeginDelta(scriptedFrameTime);
  } else {
    rlImGuiBegin();
  }
  ImGuizmo::BeginFrame();
  buildInterface();
  viewport.render();
  rlImGuiEnd();
  finishScreenshot();
  EndDrawing();
}

void EditorApp::pollFiles() {
  if (imageWatcher.hasChanged()) {
    viewport.reloadTextures();
    assets.unloadThumbnails();
    session.lastReload = "Reloaded textures";
    TraceLog(LOG_INFO, "ASSETS: Image files changed; textures reloaded");
  }
  if (diskPoll.elapsed()) {
    session.pollDisk();
  }
}

void EditorApp::frameNewDocument() {
  if (session.doc().id() != framedDocument) {
    framedDocument = session.doc().id();
    viewport.frameObjects({});
  }
}

void EditorApp::buildInterface() {
  const UiContext ui = {session, viewport, dialogs};
  ScriptTargets().clear();
  DrawMenuBar(ui, preferences, resetLayout);

  const ImGuiID dockspace =
      ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
  if (resetLayout) {
    resetLayout = false;
    ApplyDefaultLayout(dockspace);
  }

  ImGui::Begin(
      viewportWindow, nullptr,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  DrawToolbar(ui);
  viewport.build();
  ImGui::End();

  ImGui::Begin(explorerWindow);
  explorer.draw(ui);
  ImGui::End();

  ImGui::Begin(propertiesWindow);
  properties.draw(ui);
  ImGui::End();

  ImGui::Begin(assetsWindow);
  assets.draw(ui);
  ImGui::End();

  ImGui::Begin(logWindow);
  log.draw();
  ImGui::End();

  dialogs.draw(session);
  HandleShortcuts(ui);
}

void EditorApp::finishScreenshot() {
  if (!viewport.hasPendingScreenshot()) {
    return;
  }
  std::string error;
  const bool saved = viewport.finishScreenshot(error);
  if (!saved) {
    TraceLog(LOG_ERROR, "SCENIC: %s", error.c_str());
  }
  if (liveRequests.isAwaitingFrame()) {
    liveRequests.finishFrame(saved, error);
  } else if (!saved && script.isActive()) {
    script.fail(error);
  }
}
