#ifndef GBM_TOUCH_CONTROLS_H
#define GBM_TOUCH_CONTROLS_H

#include <string>
#include <vector>

#include "raylib.h"

// Positions are fractions of the screen size and radius is a fraction of the
// screen height, so on-screen controls keep their place when the window or
// canvas is resized.
struct TouchButton {
  Vector2 anchor;
  float radius;
  std::string label;
};

struct TouchStick {
  Vector2 anchor;
  float radius;
};

// Pointer capture and displacement for one bound stick.
struct BoundTouchStick {
  TouchStick stick;
  int pointer = -2;
  Vector2 value = {0.0f, 0.0f};
};

// Owns pointer sampling and touch presentation, independently of action maps.
// The input system decides which controls are live and passes them in.
class TouchControls {
 public:
  void updatePointers();
  void force(bool forced);
  void notePhysicalInput();
  bool isShowing() const;

  bool isButtonDown(const TouchButton& button) const;
  void updateStick(BoundTouchStick& bound, bool live) const;
  void drawButton(const TouchButton& button, bool down) const;
  void drawStick(const BoundTouchStick& bound) const;

 private:
  struct Pointer {
    int id;
    Vector2 position;
  };

  std::vector<Pointer> pointers;
  bool touchSeen = false;
  bool touchForced = false;
};

#endif  // !GBM_TOUCH_CONTROLS_H
