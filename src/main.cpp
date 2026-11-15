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

// Scenes hold GPU resources, so the game has to be destroyed before the window closes
static bool RunGame(const LaunchOptions &options)
{
  Game game;
  if (!options.script.empty() && !game.playtest.load(options.script))
  {
    return false;
  }
  if (!PushSceneByName(game.scenes, options.scene))
  {
    TraceLog(LOG_ERROR, "GAME: Unknown scene [%s]", options.scene.c_str());
    return false;
  }

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop_arg(UpdateAndDrawFrame, &game, 0, 1);
#else
  // Scripted runs are left uncapped so they finish as fast as the machine allows
  SetTargetFPS(game.playtest.isActive() ? 0 : 60);
  while (!WindowShouldClose() && !game.scenes.isDone() && !game.playtest.isFinished())
  {
    UpdateAndDrawFrame(&game);
  }
#endif
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
    // Keeps a scripted run from stealing keyboard focus from whoever started it
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
