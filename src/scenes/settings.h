#ifndef GBM_SETTINGS_H
#define GBM_SETTINGS_H

#include "core/scenes/scene.h"
#include "core/ui.h"

class SettingsScene : public Scene {
 public:
  explicit SettingsScene(SceneManager& scenes);

  const char* name() const override { return "settings"; }
  std::string debugState() const override;
  void update(float deltaTime) override;
  void draw() const override;

 private:
  Menu menu;
  int volumeItem = 0;
  int fullscreenItem = -1;
};

#endif  // !GBM_SETTINGS_H
