#include "scenes/settings.h"

#include <cmath>

#include "core/scene_manager.h"

namespace
{
const Color backgroundColor = {11, 15, 31, 255};

void CycleVolume()
{
  const float next = std::round(GetMasterVolume() * 10.0f + 1.0f) / 10.0f;
  SetMasterVolume(next > 1.0f ? 0.0f : next);
}
} // namespace

SettingsScene::SettingsScene(SceneManager &scenes) : Scene(scenes)
{
  volumeItem = menu.add("", CycleVolume);
#if !defined(PLATFORM_WEB)
  fullscreenItem = menu.add("", ToggleBorderlessWindowed);
#endif
  menu.add("Back", [this] { this->scenes.pop(); });
}

std::string SettingsScene::debugState() const
{
  return "selected=" + menu.selectedLabel();
}

void SettingsScene::update(float)
{
  menu.setLabel(volumeItem, TextFormat("Volume: %d%%", (int)std::round(GetMasterVolume() * 100.0f)));
  if (fullscreenItem >= 0)
  {
    const bool fullscreen = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    menu.setLabel(fullscreenItem, fullscreen ? "Fullscreen: On" : "Fullscreen: Off");
  }

  menu.update();
  if (IsKeyPressed(KEY_ESCAPE))
  {
    scenes.pop();
  }
}

void SettingsScene::draw() const
{
  ClearBackground(backgroundColor);
  DrawTextCentered("Settings", 0.25f, 48, RAYWHITE);
  menu.draw();
}
