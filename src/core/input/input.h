#ifndef GBM_INPUT_H
#define GBM_INPUT_H

#include <initializer_list>
#include <string>

#include "core/input/touch_controls.h"
#include "raylib.h"

// Game code asks about named actions; src/game/bindings.cpp decides which keys,
// gamepad buttons, gestures and on-screen controls trigger them. Asking about
// an action nobody bound is a fatal error in Debug.

void BindKeys(const std::string& action, std::initializer_list<int> keys);
void BindGamepadButtons(const std::string& action,
                        std::initializer_list<int> buttons);
void BindGesture(const std::string& action, int gesture);
void BindTouchButton(const std::string& action, const TouchButton& button);

// Axes point right and down, like the screen and like gamepad sticks, and never
// exceed a length of 1
void BindAxisKeys(const std::string& axis, int left, int right, int up,
                  int down);
void BindAxisGamepadStick(const std::string& axis, int axisX, int axisY);
void BindAxisTouchStick(const std::string& axis, const TouchStick& stick);

bool ActionPressed(const std::string& action);
bool ActionDown(const std::string& action);
bool ActionReleased(const std::string& action);
Vector2 ActionAxis(const std::string& axis);

void UpdateInput();

// An on-screen control is only active and drawn while a scene is asking about
// its action, and only once the player has touched the screen. Forcing them on
// lets a mouse stand in for a finger on desktop.
void ForceTouchControls(bool forced);
void DrawTouchControls();

#endif  // !GBM_INPUT_H
