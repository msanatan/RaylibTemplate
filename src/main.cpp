#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/assets.h"
#include "core/launch_options.h"
#include "core/playtest.h"
#include "core/scene_manager.h"
#include "raylib.h"
#include "scenes/registry.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

struct Game
{
  SceneManager scenes;
  Playtest playtest;
};

static void UpdateAndDrawFrame(void *gamePointer)
{
  Game &game = *static_cast<Game *>(gamePointer);
  if (!game.playtest.isActive())
  {
    PollAssetChanges([&game] { game.scenes.reload(); });
  }
  game.playtest.beginFrame(game.scenes);
  game.scenes.update(game.playtest.isActive() ? Playtest::frameTime : GetFrameTime());

  BeginDrawing();
  game.scenes.draw();
  game.playtest.endFrame();
  EndDrawing();
}

static bool PushStartingScenes(SceneManager &scenes, const LaunchOptions &options)
{
  std::ifstream sessionFile(options.session);
  if (!options.session.empty() && sessionFile)
  {
    std::stringstream session;
    session << sessionFile.rdbuf();
    if (scenes.restoreSession(session.str()))
    {
      return true;
    }
    TraceLog(LOG_WARNING, "GAME: Ignoring session [%s], it is empty or names an unregistered scene", options.session.c_str());
  }

  std::istringstream names(options.scene);
  std::string name;
  while (std::getline(names, name, ','))
  {
    if (!scenes.pushNamed(name))
    {
      TraceLog(LOG_ERROR, "GAME: Unknown scene [%s]", name.c_str());
      return false;
    }
  }
  return true;
}

// Scenes hold GPU resources, so the game has to be destroyed before the window closes
static bool RunGame(const LaunchOptions &options)
{
  Game game;
  RegisterScenes(game.scenes);
  if (!options.script.empty() && !game.playtest.load(options.script))
  {
    return false;
  }
  if (!PushStartingScenes(game.scenes, options))
  {
    return false;
  }

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop_arg(UpdateAndDrawFrame, &game, 0, 1);
#else
  const int uncapped = 0;
  SetTargetFPS(game.playtest.isActive() ? uncapped : 60);
  while (!WindowShouldClose() && !game.scenes.isDone() && !game.playtest.isFinished())
  {
    UpdateAndDrawFrame(&game);
  }
#endif

  const std::string session = game.scenes.saveSession();
  if (!options.session.empty() && !session.empty())
  {
    std::ofstream(options.session) << session;
  }
  return !game.playtest.hasFailed();
}

int main(int argc, char **argv)
{
  LaunchOptions options;
  std::string error;
  if (!ParseLaunchOptions(std::vector<std::string>(argv + 1, argv + argc), options, error))
  {
    TraceLog(LOG_ERROR, "GAME: %s", error.c_str());
    return 1;
  }

  const int screenWidth = 1024;
  const int screenHeight = 768;
#if defined(PLATFORM_WEB)
  // raylib resizes the canvas to fill the browser window (or itch.io iframe) when this is set
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
  if (!options.script.empty())
  {
    SetConfigFlags(FLAG_WINDOW_UNFOCUSED);
  }
  InitWindow(screenWidth, screenHeight, GAME_TITLE);
  InitAudioDevice();
  InitAssets();
  // Esc opens the pause menu, so it can't also be raylib's quit shortcut
  SetExitKey(KEY_NULL);

  const bool succeeded = RunGame(options);

  CloseAssets();
  CloseAudioDevice();
  CloseWindow();
  return succeeded ? 0 : 1;
}
