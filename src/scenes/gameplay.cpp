#include "scenes/gameplay.h"

#include "core/assets/assets.h"
#include "core/input/input.h"
#include "core/scenes/scene_manager.h"
#include "levels/runtime/loading.h"
#include "rlgl.h"
#include "scenes/pause.h"

namespace {
const float playerSpeed = 5.0f;
const char* const levelFile = "levels/main.json";

// The view used when a level designates no camera
Camera3D DefaultCamera() {
  Camera3D camera = {};
  camera.position = {0.0f, 10.0f, 10.0f};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;
  return camera;
}
}  // namespace

GameplayScene::GameplayScene(SceneManager& scenes)
    : Scene(scenes),
      world(LoadGameLevel(AssetPath(levelFile)).level, DefaultCamera()),
      renderer(AssetPath("")) {}

std::string GameplayScene::debugState() const {
  const Vector3 position = world.player().position;
  return TextFormat("player=(%.2f, %.2f, %.2f)", position.x, position.y,
                    position.z);
}

std::string GameplayScene::saveState() const { return world.saveState(); }

void GameplayScene::restoreState(const std::string& state) {
  world.restoreState(state);
}

void GameplayScene::update(float deltaTime) {
  if (ActionPressed("pause")) {
    scenes.push<PauseScene>();
    return;
  }
  if (ActionPressed("reset_level")) {
    // A new scene carries no saved state, so the level starts as authored
    TraceLog(LOG_INFO, "LEVEL: Reset to the authored starting state");
    scenes.replace<GameplayScene>();
    return;
  }

  const Vector2 move = ActionAxis("move");
  world.movePlayer({move.x * playerSpeed * deltaTime, 0.0f,
                    move.y * playerSpeed * deltaTime});
}

void GameplayScene::draw() const {
  ClearBackground(RAYWHITE);
  if (world.lens().has_value()) {
    ApplyLevelCameraClipping(*world.lens());
  } else {
    rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
  }
  BeginMode3D(world.camera());
  renderer.draw(world.level());
  if (world.hasPlayer()) {
    // The unlit cube has no shading, so an outline keeps its shape readable
    const float size = world.player().size;
    DrawCubeWires(world.player().position, size, size, size, MAROON);
  }
  EndMode3D();
  DrawFPS(10, 10);
  DrawText("Esc to pause", 10, 36, 20, GRAY);
}
