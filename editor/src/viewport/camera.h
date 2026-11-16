#ifndef SCENIC_EDITOR_CAMERA_H
#define SCENIC_EDITOR_CAMERA_H

#include "raylib.h"

struct EditorCamera {
  Vector3 target = {0.0f, 0.0f, 0.0f};
  float yaw = 0.6f;
  float pitch = 0.5f;
  float distance = 16.0f;

  Camera3D view() const;
  void orbit(Vector2 pixels);
  void look(Vector2 pixels);
  void pan(Vector2 pixels);
  void zoom(float wheel);
  void fly(Vector3 direction, float seconds, bool fast);
  void frame(Vector3 center, float radius);
};

#endif  // !SCENIC_EDITOR_CAMERA_H
