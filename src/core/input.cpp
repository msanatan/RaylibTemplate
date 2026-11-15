#include "core/input.h"

#include <array>
#include <map>
#include <vector>

#include "raymath.h"

namespace {
const float stickDeadzone = 0.2f;
const float stickCaptureReach = 1.5f;
const int maxGamepads = 4;
const int mousePointer = -1;
const int noPointer = -2;

struct Pointer {
  int id;
  Vector2 position;
};

struct ButtonAction {
  std::vector<int> keys;
  std::vector<int> gamepadButtons;
  std::vector<int> gestures;
  std::vector<TouchButton> touchButtons;
  bool down = false;
  bool wasDown = false;
  int lastAskedFrame = -1;
};

struct BoundTouchStick {
  TouchStick stick;
  int pointer = noPointer;
  Vector2 value = {0.0f, 0.0f};
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
std::vector<Pointer> pointers;
int frame = 0;
bool touchSeen = false;
bool touchForced = false;

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

bool TouchControlsShowing() { return touchForced || touchSeen; }

template <typename Action>
bool IsLive(const Action& action) {
  return TouchControlsShowing() && action.lastAskedFrame >= frame - 1;
}

Vector2 Center(Vector2 anchor) {
  return {anchor.x * (float)GetScreenWidth(),
          anchor.y * (float)GetScreenHeight()};
}

float Radius(float radius) { return radius * (float)GetScreenHeight(); }

void UpdatePointers() {
  pointers.clear();
  for (int i = 0; i < GetTouchPointCount(); i++) {
    pointers.push_back({GetTouchPointId(i), GetTouchPosition(i)});
  }
  touchSeen = touchSeen || !pointers.empty();
  if (pointers.empty() && touchForced && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    pointers.push_back({mousePointer, GetMousePosition()});
  }
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
    // Picking up a keyboard or controller puts the on-screen controls away
    // again
    touchSeen = false;
    return true;
  }

  for (int gesture : action.gestures) {
    if (IsGestureDetected((unsigned int)gesture)) {
      return true;
    }
  }
  if (IsLive(action)) {
    for (const TouchButton& button : action.touchButtons) {
      for (const Pointer& pointer : pointers) {
        if (CheckCollisionPointCircle(pointer.position, Center(button.anchor),
                                      Radius(button.radius))) {
          return true;
        }
      }
    }
  }
  return false;
}

void UpdateTouchStick(BoundTouchStick& bound, bool live) {
  const Vector2 center = Center(bound.stick.anchor);
  const float radius = Radius(bound.stick.radius);
  const Pointer* holder = nullptr;
  for (const Pointer& pointer : pointers) {
    const bool inReach =
        Vector2Distance(pointer.position, center) <= radius * stickCaptureReach;
    // A finger that started on the stick keeps steering it however far it then
    // drags
    if (pointer.id == bound.pointer ||
        (bound.pointer == noPointer && inReach)) {
      holder = &pointer;
      break;
    }
  }

  if (!live || holder == nullptr) {
    bound.pointer = noPointer;
    bound.value = {0.0f, 0.0f};
    return;
  }
  bound.pointer = holder->id;
  bound.value = Vector2ClampValue(
      Vector2Scale(Vector2Subtract(holder->position, center), 1.0f / radius),
      0.0f, 1.0f);
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
    touchSeen = false;
  }

  for (BoundTouchStick& bound : action.touchSticks) {
    UpdateTouchStick(bound, IsLive(action));
    value = Vector2Add(value, bound.value);
  }
  return Vector2ClampValue(value, 0.0f, 1.0f);
}

void DrawLabel(const std::string& label, Vector2 center, float radius) {
  const int fontSize = (int)radius;
  const int width = MeasureText(label.c_str(), fontSize);
  DrawText(label.c_str(), (int)center.x - width / 2,
           (int)center.y - fontSize / 2, fontSize, Fade(BLACK, 0.6f));
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
  UpdatePointers();
  for (auto& [name, action] : buttons) {
    action.wasDown = action.down;
    action.down = IsButtonDown(action);
  }
  for (auto& [name, action] : axes) {
    action.value = ReadAxis(action);
  }
}

void ForceTouchControls(bool forced) { touchForced = forced; }

void DrawTouchControls() {
  for (const auto& [name, action] : buttons) {
    for (const TouchButton& button : action.touchButtons) {
      if (IsLive(action)) {
        const Vector2 center = Center(button.anchor);
        DrawCircleV(center, Radius(button.radius),
                    Fade(GRAY, action.down ? 0.6f : 0.35f));
        DrawCircleLinesV(center, Radius(button.radius), Fade(BLACK, 0.4f));
        DrawLabel(button.label, center, Radius(button.radius));
      }
    }
  }
  for (const auto& [name, action] : axes) {
    for (const BoundTouchStick& bound : action.touchSticks) {
      if (IsLive(action)) {
        const Vector2 center = Center(bound.stick.anchor);
        const float radius = Radius(bound.stick.radius);
        DrawCircleV(center, radius, Fade(GRAY, 0.25f));
        DrawCircleLinesV(center, radius, Fade(BLACK, 0.4f));
        DrawCircleV(Vector2Add(center, Vector2Scale(bound.value, radius)),
                    radius * 0.45f, Fade(GRAY, 0.6f));
      }
    }
  }
}
