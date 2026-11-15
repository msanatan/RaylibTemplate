#ifndef GBM_MAIN_MENU_H
#define GBM_MAIN_MENU_H

#include "core/scene.h"
#include "core/ui.h"

class MainMenuScene : public Scene
{
public:
  explicit MainMenuScene(SceneManager &scenes);

  const char *name() const override { return "main_menu"; }
  std::string debugState() const override;
  void update(float deltaTime) override;
  void draw() const override;

private:
  Menu menu;
};

#endif // !GBM_MAIN_MENU_H
