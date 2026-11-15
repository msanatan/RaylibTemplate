#ifndef GBM_SETTINGS_H
#define GBM_SETTINGS_H

#include "core/scene.h"
#include "core/ui.h"

class SettingsScene : public Scene
{
public:
  explicit SettingsScene(SceneManager &scenes);

  void update(float deltaTime) override;
  void draw() const override;

private:
  Menu menu;
  int volumeItem = 0;
  int fullscreenItem = -1;
};

#endif // !GBM_SETTINGS_H
