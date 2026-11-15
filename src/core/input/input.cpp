#include "core/input/input.h"

#include <array>
#include <map>
#include <vector>

#include "raymath.h"

namespace {
const float stickDeadzone = 0.2f;
const int maxGamepads = 4;

struct ButtonAction {
  std::vector<int> keys;
  std::vector<int> gamepadButtons;
  std::vector<int> gestures;
  std::vector<TouchButton> touchButtons;
  bool down = false;
  bool wasDown = false;
  int lastAskedFrame = -1;
};

struct AxisAction {
  std::vector<std::array<int, 4>> keys;
  std::vector<std::array<int, 2>> gamepadSticks;
  std::vector<BoundTouchStick> touchSticks;
  Vector2 value = {0.0f, 0.0f};
  int lastAskedFrame = -1;
};

std::map<std::string, ButtonAction> buttons;
std::map<std::string, AxisAction> axes;
int frame = 0;
TouchControls touchControls;

template <typename Action>
Action& Asked(std::map<std::string, Action>& actions, const std::string& name) {
  const auto action = actions.find(name);
  if (action == actions.end()) {
#if defined(NDEBUG)
    TraceLog(LOG_ERROR, "INPUT: Nothing is bound to [%s]", name.c_str());
#else
    TraceLog(LOG_FATAL, "INPUT: Nothing is bound to [%s]", name.c_str());
#endif
    return actions[name];
  }
  action->second.lastAskedFrame = frame;
  return action->second;
}

template <typename Action>
bool IsLive(const Action& action) {
  return touchControls.isShowing() && action.lastAskedFrame >= frame - 1;
}

bool IsButtonDown(const ButtonAction& action) {
  bool physical = false;
  for (int key : action.keys) {
    physical = physical || IsKeyDown(key);
  }
  for (int button : action.gamepadButtons) {
    for (int gamepad = 0; gamepad < maxGamepads; gamepad++) {
      physical = physical || (IsGamepadAvailable(gamepad) &&
                              IsGamepadButtonDown(gamepad, button));
    }
  }
  if (physical) {
    touchControls.notePhysicalInput();
    return true;
  }

  for (int gesture : action.gestures) {
    if (IsGestureDetected((unsigned int)gesture)) {
      return true;
    }
  }
  if (IsLive(action)) {
    for (const TouchButton& button : action.touchButtons) {
      if (touchControls.isButtonDown(button)) {
        return true;
      }
    }
  }
  return false;
}

Vector2 ReadAxis(AxisAction& action) {
  Vector2 value = {0.0f, 0.0f};
  for (const std::array<int, 4>& keys : action.keys) {
    value.x += (float)IsKeyDown(keys[1]) - (float)IsKeyDown(keys[0]);
    value.y += (float)IsKeyDown(keys[3]) - (float)IsKeyDown(keys[2]);
  }
  for (const std::array<int, 2>& stick : action.gamepadSticks) {
    for (int gamepad = 0; gamepad < maxGamepads; gamepad++) {
      if (!IsGamepadAvailable(gamepad)) {
        continue;
      }
      const Vector2 tilt = {GetGamepadAxisMovement(gamepad, stick[0]),
                            GetGamepadAxisMovement(gamepad, stick[1])};
      if (Vector2Length(tilt) > stickDeadzone) {
        value = Vector2Add(value, tilt);
      }
    }
  }
  if (Vector2Length(value) > 0.0f) {
    touchControls.notePhysicalInput();
  }

  for (BoundTouchStick& bound : action.touchSticks) {
    touchControls.updateStick(bound, IsLive(action));
    value = Vector2Add(value, bound.value);
  }
  return Vector2ClampValue(value, 0.0f, 1.0f);
}

}  // namespace

void BindKeys(const std::string& action, std::initializer_list<int> keys) {
  buttons[action].keys.insert(buttons[action].keys.end(), keys);
}

void BindGamepadButtons(const std::string& action,
                        std::initializer_list<int> gamepadButtons) {
  buttons[action].gamepadButtons.insert(buttons[action].gamepadButtons.end(),
                                        gamepadButtons);
}

void BindGesture(const std::string& action, int gesture) {
  buttons[action].gestures.push_back(gesture);
}

void BindTouchButton(const std::string& action, const TouchButton& button) {
  buttons[action].touchButtons.push_back(button);
}

void BindAxisKeys(const std::string& axis, int left, int right, int up,
                  int down) {
  axes[axis].keys.push_back({left, right, up, down});
}

void BindAxisGamepadStick(const std::string& axis, int axisX, int axisY) {
  axes[axis].gamepadSticks.push_back({axisX, axisY});
}

void BindAxisTouchStick(const std::string& axis, const TouchStick& stick) {
  axes[axis].touchSticks.push_back({stick});
}

bool ActionPressed(const std::string& action) {
  const ButtonAction& button = Asked(buttons, action);
  return button.down && !button.wasDown;
}

bool ActionDown(const std::string& action) {
  return Asked(buttons, action).down;
}

bool ActionReleased(const std::string& action) {
  const ButtonAction& button = Asked(buttons, action);
  return !button.down && button.wasDown;
}

Vector2 ActionAxis(const std::string& axis) { return Asked(axes, axis).value; }

void UpdateInput() {
  frame++;
  touchControls.updatePointers();
  for (auto& [name, action] : buttons) {
    action.wasDown = action.down;
    action.down = IsButtonDown(action);
  }
  for (auto& [name, action] : axes) {
    action.value = ReadAxis(action);
  }
}

void ForceTouchControls(bool forced) { touchControls.force(forced); }

void DrawTouchControls() {
  for (const auto& [name, action] : buttons) {
    for (const TouchButton& button : action.touchButtons) {
      if (IsLive(action)) {
        touchControls.drawButton(button, action.down);
      }
    }
  }
  for (const auto& [name, action] : axes) {
    for (const BoundTouchStick& bound : action.touchSticks) {
      if (IsLive(action)) {
        touchControls.drawStick(bound);
      }
    }
  }
}
