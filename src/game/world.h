#ifndef GBM_WORLD_H
#define GBM_WORLD_H

#include <optional>
#include <string>

#include "game/player.h"
#include "levels/document/level.h"
#include "levels/document/level_index.h"
#include "raylib.h"

class World {
 public:
  World(const Level& authored, const Camera3D& fallbackCamera);

  World(const World&) = delete;
  World& operator=(const World&) = delete;

  const LevelIndex& level() const { return index; }

  bool hasPlayer() const { return !playerId.empty(); }
  const Player& player() const { return playerState; }
  void movePlayer(Vector3 velocity);

  const Camera3D& camera() const { return view; }
  const std::optional<LevelCameraSettings>& lens() const { return cameraLens; }
  // Until game code takes control, the camera follows the authored one, so
  // saved camera edits show up on reload. Afterwards the gameplay view is
  // kept across reloads.
  void controlCamera(const Camera3D& camera);
  bool isCameraGameControlled() const { return cameraGameControlled; }

  std::string saveState() const;
  void restoreState(const std::string& state);

 private:
  void placePlayer(Vector3 position);

  Level current;
  LevelIndex index;
  std::string playerId;
  Player playerState = Player(0.0f, 0.0f, 0.0f, 2.0f);
  Camera3D view;
  std::optional<LevelCameraSettings> cameraLens;
  bool cameraGameControlled = false;
};

#endif  // !GBM_WORLD_H
