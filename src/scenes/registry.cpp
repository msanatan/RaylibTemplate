#include "scenes/registry.h"

#include <functional>
#include <map>

#include "scenes/gameplay.h"
#include "scenes/main_menu.h"
#include "scenes/pause.h"
#include "scenes/settings.h"
#include "scenes/splash.h"

bool PushSceneByName(SceneManager &scenes, const std::string &name)
{
  static const std::map<std::string, std::function<void(SceneManager &)>> starters = {
      {"splash", [](SceneManager &s) { s.push<SplashScene>(); }},
      {"main_menu", [](SceneManager &s) { s.push<MainMenuScene>(); }},
      {"settings", [](SceneManager &s) { s.push<SettingsScene>(); }},
      {"gameplay", [](SceneManager &s) { s.push<GameplayScene>(); }},
      {"pause", [](SceneManager &s) {
         s.push<GameplayScene>();
         s.push<PauseScene>();
       }},
  };

  const auto starter = starters.find(name);
  if (starter == starters.end())
  {
    return false;
  }
  starter->second(scenes);
  return true;
}
