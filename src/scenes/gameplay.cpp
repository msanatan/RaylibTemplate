#include "scenes/gameplay.h"

#include "core/scene_manager.h"
#include "raymath.h"
#include "scenes/pause.h"

namespace
{
const float playerSpeed = 5.0f;
}

GameplayScene::GameplayScene(SceneManager &scenes) : Scene(scenes)
{
  camera.position = {0.0f, 10.0f, 10.0f};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;
}

std::string GameplayScene::debugState() const
{
  return TextFormat("player=(%.2f, %.2f, %.2f)", player.position.x, player.position.y, player.position.z);
}

void GameplayScene::update(float deltaTime)
{
  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P))
  {
    scenes.push<PauseScene>();
    return;
  }

  Vector3 direction = {0.0f, 0.0f, 0.0f};
  if (IsKeyDown(KEY_LEFT))
    direction.x -= 1.0f;
  if (IsKeyDown(KEY_RIGHT))
    direction.x += 1.0f;
  if (IsKeyDown(KEY_UP))
    direction.z -= 1.0f;
  if (IsKeyDown(KEY_DOWN))
    direction.z += 1.0f;

  player.move(Vector3Scale(Vector3Normalize(direction), playerSpeed * deltaTime));
}

void GameplayScene::draw() const
{
  ClearBackground(RAYWHITE);
  BeginMode3D(camera);
  player.draw();
  EndMode3D();
  DrawFPS(10, 10);
  DrawText("Esc to pause", 10, 36, 20, GRAY);
}
