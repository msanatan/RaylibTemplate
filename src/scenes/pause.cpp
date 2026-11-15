#include "scenes/pause.h"

#include "core/scene_manager.h"
#include "scenes/main_menu.h"
#include "scenes/settings.h"

PauseScene::PauseScene(SceneManager &scenes) : Scene(scenes)
{
  menu.add("Resume", [this] { this->scenes.pop(); });
  menu.add("Settings", [this] { this->scenes.push<SettingsScene>(); });
  menu.add("Main Menu", [this] { this->scenes.reset<MainMenuScene>(); });
}

void PauseScene::update(float)
{
  menu.update();
  if (IsKeyPressed(KEY_ESCAPE))
  {
    scenes.pop();
  }
}

void PauseScene::draw() const
{
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
  DrawTextCentered("Paused", 0.25f, 48, RAYWHITE);
  menu.draw();
}
