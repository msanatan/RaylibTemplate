#ifndef GBM_PAUSE_H
#define GBM_PAUSE_H

#include "core/scenes/scene.h"
#include "core/ui.h"

class PauseScene : public Scene {
 public:
  explicit PauseScene(SceneManager& scenes);

  const char* name() const override { return "pause"; }
  std::string debugState() const override;
  void update(float deltaTime) override;
  void draw() const override;
  bool isOverlay() const override { return true; }

 private:
  Menu menu;
};

#endif  // !GBM_PAUSE_H
