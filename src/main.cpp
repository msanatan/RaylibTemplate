#include "core/assets.h"
#include "core/scene_manager.h"
#include "raylib.h"
#include "scenes/splash.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static void UpdateAndDrawFrame(void *sceneManager)
{
  SceneManager &scenes = *static_cast<SceneManager *>(sceneManager);
  scenes.update(GetFrameTime());

  BeginDrawing();
  scenes.draw();
  EndDrawing();
}

// Scenes hold GPU resources, so the manager has to be destroyed before the window closes
static void RunGame()
{
  SceneManager scenes;
  scenes.push<SplashScene>();

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop_arg(UpdateAndDrawFrame, &scenes, 0, 1);
#else
  SetTargetFPS(60);
  while (!WindowShouldClose() && !scenes.isDone())
  {
    UpdateAndDrawFrame(&scenes);
  }
#endif
}

int main()
{
  const int screenWidth = 1024;
  const int screenHeight = 768;
#if defined(PLATFORM_WEB)
  // raylib resizes the canvas to fill the browser window (or itch.io iframe) when this is set
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
  InitWindow(screenWidth, screenHeight, GAME_TITLE);
  InitAudioDevice();
  InitAssets();
  // Esc opens the pause menu, so it can't also be raylib's quit shortcut
  SetExitKey(KEY_NULL);

  RunGame();

  CloseAssets();
  CloseAudioDevice();
  CloseWindow();
  return 0;
}
