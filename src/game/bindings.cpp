#include "game/bindings.h"

#include "core/input/input.h"

void BindGameInput() {
  // The ui_* actions are the ones core/ui.cpp asks about, so every game needs
  // them
  BindKeys("ui_up", {KEY_UP, KEY_W});
  BindGamepadButtons("ui_up", {GAMEPAD_BUTTON_LEFT_FACE_UP});
  BindKeys("ui_down", {KEY_DOWN, KEY_S});
  BindGamepadButtons("ui_down", {GAMEPAD_BUTTON_LEFT_FACE_DOWN});
  BindKeys("ui_accept", {KEY_ENTER, KEY_SPACE});
  BindGamepadButtons("ui_accept", {GAMEPAD_BUTTON_RIGHT_FACE_DOWN});
  BindKeys("ui_back", {KEY_ESCAPE});
  BindGamepadButtons("ui_back", {GAMEPAD_BUTTON_RIGHT_FACE_RIGHT});

  BindKeys("pause", {KEY_ESCAPE, KEY_P});
  BindGamepadButtons("pause", {GAMEPAD_BUTTON_MIDDLE_RIGHT});
  BindTouchButton("pause", {{0.93f, 0.09f}, 0.05f, "II"});

  // Reloads the level's authored starting state while iterating on it. Only
  // desktop Debug builds bind a key, so players never trigger it.
#if defined(DEV_ASSETS_DIR)
  BindKeys("reset_level", {KEY_F5});
#else
  BindKeys("reset_level", {});
#endif

  BindAxisKeys("move", KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN);
  BindAxisKeys("move", KEY_A, KEY_D, KEY_W, KEY_S);
  BindAxisGamepadStick("move", GAMEPAD_AXIS_LEFT_X, GAMEPAD_AXIS_LEFT_Y);
  BindAxisTouchStick("move", {{0.15f, 0.8f}, 0.12f});
}
