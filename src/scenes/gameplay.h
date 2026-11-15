#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include "core/scene.h"
#include "game/player.h"
#include "raylib.h"

class GameplayScene : public Scene
{
public:
  explicit GameplayScene(SceneManager &scenes);

  void update(float deltaTime) override;
  void draw() const override;

private:
  Camera3D camera = {};
  Player player = Player(0.0f, 0.0f, 0.0f, 2.0f);
};

#endif // !GAMEPLAY_H
