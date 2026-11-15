#include "core/input/touch_controls.h"

#include "raymath.h"

namespace {
const float stickCaptureReach = 1.5f;
const int mousePointer = -1;
const int noPointer = -2;

Vector2 Center(Vector2 anchor) {
  return {anchor.x * (float)GetScreenWidth(),
          anchor.y * (float)GetScreenHeight()};
}

float Radius(float radius) { return radius * (float)GetScreenHeight(); }

void DrawLabel(const std::string& label, Vector2 center, float radius) {
  const int fontSize = (int)radius;
  const int width = MeasureText(label.c_str(), fontSize);
  DrawText(label.c_str(), (int)center.x - width / 2,
           (int)center.y - fontSize / 2, fontSize, Fade(BLACK, 0.6f));
}
}  // namespace

void TouchControls::updatePointers() {
  pointers.clear();
  for (int i = 0; i < GetTouchPointCount(); i++) {
    pointers.push_back({GetTouchPointId(i), GetTouchPosition(i)});
  }
  touchSeen = touchSeen || !pointers.empty();
  if (pointers.empty() && touchForced && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    pointers.push_back({mousePointer, GetMousePosition()});
  }
}

void TouchControls::force(bool forced) { touchForced = forced; }

void TouchControls::notePhysicalInput() {
  // Picking up a keyboard or controller puts the on-screen controls away again.
  touchSeen = false;
}

bool TouchControls::isShowing() const { return touchForced || touchSeen; }

bool TouchControls::isButtonDown(const TouchButton& button) const {
  for (const Pointer& pointer : pointers) {
    if (CheckCollisionPointCircle(pointer.position, Center(button.anchor),
                                  Radius(button.radius))) {
      return true;
    }
  }
  return false;
}

void TouchControls::updateStick(BoundTouchStick& bound, bool live) const {
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

void TouchControls::drawButton(const TouchButton& button, bool down) const {
  const Vector2 center = Center(button.anchor);
  const float radius = Radius(button.radius);
  DrawCircleV(center, radius, Fade(GRAY, down ? 0.6f : 0.35f));
  DrawCircleLinesV(center, radius, Fade(BLACK, 0.4f));
  DrawLabel(button.label, center, radius);
}

void TouchControls::drawStick(const BoundTouchStick& bound) const {
  const Vector2 center = Center(bound.stick.anchor);
  const float radius = Radius(bound.stick.radius);
  DrawCircleV(center, radius, Fade(GRAY, 0.25f));
  DrawCircleLinesV(center, radius, Fade(BLACK, 0.4f));
  DrawCircleV(Vector2Add(center, Vector2Scale(bound.value, radius)),
              radius * 0.45f, Fade(GRAY, 0.6f));
}
