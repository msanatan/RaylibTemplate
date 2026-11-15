#include "scenes/registry.h"

#include "scenes/gameplay.h"
#include "scenes/main_menu.h"
#include "scenes/pause.h"
#include "scenes/settings.h"
#include "scenes/splash.h"

void RegisterScenes(SceneManager &scenes)
{
  scenes.registerScene<SplashScene>("splash");
  scenes.registerScene<MainMenuScene>("main_menu");
  scenes.registerScene<SettingsScene>("settings");
  scenes.registerScene<GameplayScene>("gameplay");
  scenes.registerScene<PauseScene>("pause");
}
