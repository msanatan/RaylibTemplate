#include "scenes/gameplay.h"

#include <sstream>

#include "core/input/input.h"
#include "core/scenes/scene_manager.h"
#include "raymath.h"
#include "scenes/pause.h"

namespace {
const float playerSpeed = 5.0f;
}

GameplayScene::GameplayScene(SceneManager& scenes) : Scene(scenes) {
  camera.position = {0.0f, 10.0f, 10.0f};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;
}

std::string GameplayScene::debugState() const {
  return TextFormat("player=(%.2f, %.2f, %.2f)", player.position.x,
                    player.position.y, player.position.z);
}

std::string GameplayScene::saveState() const {
  return TextFormat("%f %f %f", player.position.x, player.position.y,
                    player.position.z);
}

void GameplayScene::restoreState(const std::string& state) {
  std::istringstream(state) >> player.position.x >> player.position.y >>
      player.position.z;
}

void GameplayScene::update(float deltaTime) {
  if (ActionPressed("pause")) {
    scenes.push<PauseScene>();
    return;
  }

  const Vector2 move = ActionAxis("move");
  player.move({move.x * playerSpeed * deltaTime, 0.0f,
               move.y * playerSpeed * deltaTime});
}

void GameplayScene::draw() const {
  ClearBackground(RAYWHITE);
  BeginMode3D(camera);
  player.draw();
  EndMode3D();
  DrawFPS(10, 10);
  DrawText("Esc to pause", 10, 36, 20, GRAY);
}
