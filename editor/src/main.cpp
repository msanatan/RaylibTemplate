#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "app/editor_app.h"
#include "commands/editor_log.h"
#include "imgui.h"
#include "levels/editing/file_target.h"
#include "raylib.h"
#include "rlImGui.h"

namespace {
// Scripted runs use a fixed window so their clicks and screenshots repeat
const int scriptedWidth = 1600;
const int scriptedHeight = 900;

bool ParseOptions(const std::vector<std::string>& arguments,
                  EditorOptions& options, std::string& error) {
  const std::map<std::string, std::string*> values = {
      {"--project", &options.project},
      {"--level", &options.level},
      {"--script", &options.script},
      {"--log", &options.log},
  };
  for (size_t i = 0; i < arguments.size(); i += 2) {
    const auto value = values.find(arguments[i]);
    if (value == values.end()) {
      error = "Unknown option " + arguments[i];
      return false;
    }
    if (i + 1 >= arguments.size()) {
      error = "Missing value for " + arguments[i];
      return false;
    }
    *value->second = arguments[i + 1];
  }
  return true;
}

void LoadFonts() {
  ImFontConfig config;
  config.SizePixels = 15.0f;
  ImGui::GetIO().Fonts->AddFontDefaultVector(&config);
}
}  // namespace

int main(int argc, char** argv) {
  EditorOptions options;
  std::string error;
  if (!ParseOptions(std::vector<std::string>(argv + 1, argv + argc), options,
                    error)) {
    TraceLog(LOG_ERROR, "SCENIC: %s", error.c_str());
    return 1;
  }
  StartEditorLog(options.log);

  if (options.project.empty()) {
    options.project = FindProjectRoot(std::filesystem::current_path().string());
  }
  if (options.project.empty()) {
    options.project = SCENIC_DEFAULT_PROJECT;
  }
  Project project;
  if (!LoadProject(options.project, SCENIC_DEFAULT_GAME, project, error)) {
    TraceLog(LOG_ERROR, "SCENIC: %s", error.c_str());
    return 1;
  }

  const bool scripted = !options.script.empty();
  unsigned int flags = FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI |
                       FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
  if (scripted) {
    flags = FLAG_WINDOW_UNFOCUSED | FLAG_MSAA_4X_HINT;
  }
  SetConfigFlags(flags);
  InitWindow(scriptedWidth, scriptedHeight, "Scenic");
  if (!scripted) {
    MaximizeWindow();
  }
  // Escape cancels drags and closes popups; it must not also quit
  SetExitKey(KEY_NULL);
  rlImGuiSetLoadFontsCallback(LoadFonts);
  rlImGuiSetup(true);

  bool succeeded = false;
  {
    // The app owns GPU resources, so it goes before the window does
    EditorApp app(project, options);
    succeeded = app.run();
  }

  rlImGuiShutdown();
  CloseWindow();
  StopEditorLog();
  return succeeded ? 0 : 1;
}
