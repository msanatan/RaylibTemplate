#include "scenes/main_menu.h"

#include "core/scene_manager.h"
#include "scenes/gameplay.h"
#include "scenes/settings.h"

namespace {
const Color backgroundColor = {11, 15, 31, 255};
}

MainMenuScene::MainMenuScene(SceneManager& scenes) : Scene(scenes) {
  menu.add("Play", [this] { this->scenes.replace<GameplayScene>(); });
  menu.add("Settings", [this] { this->scenes.push<SettingsScene>(); });
#if !defined(PLATFORM_WEB)
  menu.add("Quit", [this] { this->scenes.quit(); });
#endif
}

std::string MainMenuScene::debugState() const {
  return "selected=" + menu.selectedLabel();
}

void MainMenuScene::update(float) { menu.update(); }

void MainMenuScene::draw() const {
  ClearBackground(backgroundColor);
  DrawTextCentered(GAME_TITLE, 0.25f, 64, RAYWHITE);
  menu.draw();
}
