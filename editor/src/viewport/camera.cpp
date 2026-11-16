#include "viewport/camera.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace {
const float turnSpeed = 0.008f;
const float panSpeed = 0.0015f;
const float pitchLimit = 1.5f;
const float fastMultiplier = 3.0f;

Vector3 FromTarget(float yaw, float pitch) {
  return {std::cos(pitch) * std::sin(yaw), std::sin(pitch),
          std::cos(pitch) * std::cos(yaw)};
}
}  // namespace

Camera3D EditorCamera::view() const {
  Camera3D camera = {};
  camera.position =
      Vector3Add(target, Vector3Scale(FromTarget(yaw, pitch), distance));
  camera.target = target;
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;
  return camera;
}

void EditorCamera::orbit(Vector2 pixels) {
  yaw -= pixels.x * turnSpeed;
  pitch = std::clamp(pitch + pixels.y * turnSpeed, -pitchLimit, pitchLimit);
}

void EditorCamera::look(Vector2 pixels) {
  const Vector3 position = view().position;
  orbit(pixels);
  target =
      Vector3Subtract(position, Vector3Scale(FromTarget(yaw, pitch), distance));
}

void EditorCamera::pan(Vector2 pixels) {
  const Camera3D camera = view();
  const Vector3 forward =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  const Vector3 right =
      Vector3Normalize(Vector3CrossProduct(forward, camera.up));
  const Vector3 up = Vector3CrossProduct(right, forward);
  // Scaled by distance so a drag moves the scene about as far on screen at
  // any zoom
  const float scale = distance * panSpeed;
  target = Vector3Add(target, Vector3Scale(right, -pixels.x * scale));
  target = Vector3Add(target, Vector3Scale(up, pixels.y * scale));
}

void EditorCamera::zoom(float wheel) {
  distance = std::clamp(distance * std::pow(0.9f, wheel), 0.2f, 5000.0f);
}

void EditorCamera::fly(Vector3 direction, float seconds, bool fast) {
  const Camera3D camera = view();
  const Vector3 forward =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  const Vector3 right =
      Vector3Normalize(Vector3CrossProduct(forward, camera.up));
  // Faster when zoomed out, so crossing a large level takes about as long as
  // crossing a small one
  const float speed =
      std::max(5.0f, distance * 0.5f) * (fast ? fastMultiplier : 1.0f);
  Vector3 movement = Vector3Scale(right, direction.x);
  movement = Vector3Add(movement, Vector3Scale(camera.up, direction.y));
  movement = Vector3Add(movement, Vector3Scale(forward, direction.z));
  target = Vector3Add(target, Vector3Scale(movement, speed * seconds));
}

void EditorCamera::frame(Vector3 center, float radius) {
  target = center;
  distance = std::max(radius * 2.5f, 2.0f);
}
