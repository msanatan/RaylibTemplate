#ifndef GBM_PAUSE_H
#define GBM_PAUSE_H

#include "core/scene.h"
#include "core/ui.h"

class PauseScene : public Scene
{
public:
  explicit PauseScene(SceneManager &scenes);

  void update(float deltaTime) override;
  void draw() const override;
  bool isOverlay() const override { return true; }

private:
  Menu menu;
};

#endif // !GBM_PAUSE_H
