#ifndef GBM_GAMEPLAY_H
#define GBM_GAMEPLAY_H

#include <string>

#include "core/scenes/scene.h"
#include "game/world.h"
#include "levels/runtime/rendering.h"

class GameplayScene : public Scene {
 public:
  explicit GameplayScene(SceneManager& scenes);

  const char* name() const override { return "gameplay"; }
  std::string debugState() const override;
  std::string saveState() const override;
  void restoreState(const std::string& state) override;
  void update(float deltaTime) override;
  void draw() const override;

 private:
  World world;
  // Drawing loads textures on first use
  mutable LevelRenderer renderer;
};

#endif  // !GBM_GAMEPLAY_H
