#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "core/scene.h"
#include "core/ui.h"

class MainMenuScene : public Scene
{
public:
  explicit MainMenuScene(SceneManager &scenes);

  void update(float deltaTime) override;
  void draw() const override;

private:
  Menu menu;
};

#endif // !MAIN_MENU_H
