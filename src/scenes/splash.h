#ifndef GBM_SPLASH_H
#define GBM_SPLASH_H

#include "core/scene.h"
#include "raylib.h"

class SplashScene : public Scene
{
public:
  explicit SplashScene(SceneManager &scenes);
  ~SplashScene() override;

  void update(float deltaTime) override;
  void draw() const override;

private:
  Texture2D logo = {};
  float elapsed = 0.0f;

  float logoAlpha() const;
};

#endif // !GBM_SPLASH_H
