#include "levels/document/transform.h"

#include <doctest/doctest.h>

#include "levels/document/level_index.h"
#include "levels/runtime/rendering.h"

namespace {
void CheckVector(const LevelVec3& actual, const LevelVec3& expected) {
  CHECK(actual.x == doctest::Approx(expected.x).epsilon(1e-9));
  CHECK(actual.y == doctest::Approx(expected.y).epsilon(1e-9));
  CHECK(actual.z == doctest::Approx(expected.z).epsilon(1e-9));
}

LevelObject Object(const std::string& id, LevelTransform transform,
                   std::optional<std::string> parent = std::nullopt) {
  LevelObject object;
  object.id = id;
  object.transform = transform;
  object.parent = std::move(parent);
  return object;
}
}  // namespace

// These pin the documented convention: Y up, -Z forward, Euler degrees
// applied as roll (Z), pitch (X), then yaw (Y), and world = parent * T * R * S.

TEST_CASE("positive yaw turns forward from -Z towards -X") {
  const LevelMatrix yaw = LocalMatrix({{0, 0, 0}, {0, 90, 0}, {1, 1, 1}});
  CheckVector(TransformDirection(yaw, levelForward), {-1, 0, 0});
}

TEST_CASE("positive pitch tilts forward up") {
  const LevelMatrix pitch = LocalMatrix({{0, 0, 0}, {90, 0, 0}, {1, 1, 1}});
  CheckVector(TransformDirection(pitch, levelForward), {0, 1, 0});
}

TEST_CASE("roll is applied before pitch, and pitch before yaw") {
  const LevelMatrix combined =
      LocalMatrix({{0, 0, 0}, {90, 90, 90}, {1, 1, 1}});
  CheckVector(TransformDirection(combined, {1, 0, 0}), {1, 0, 0});
  CheckVector(TransformDirection(combined, {0, 1, 0}), {0, 0, 1});
}

TEST_CASE("scale applies before rotation, then translation") {
  const LevelMatrix matrix = LocalMatrix({{10, 0, 0}, {0, 90, 0}, {2, 1, 1}});
  CheckVector(TransformPoint(matrix, {1, 0, 0}), {10, 0, -2});
}

TEST_CASE("a child's transform is relative to its parent") {
  Level level;
  level.objects.push_back(Object("parent", {{5, 0, 0}, {0, 90, 0}, {2, 2, 2}}));
  level.objects.push_back(
      Object("child", {{0, 0, -1}, {0, 0, 0}, {1, 1, 1}}, "parent"));

  const std::optional<LevelTransform> world =
      LevelIndex(level).worldTransform("child");
  REQUIRE(world.has_value());
  CheckVector(world->position, {3, 0, 0});
  CheckVector(world->rotation, {0, 90, 0});
  CheckVector(world->scale, {2, 2, 2});
}

TEST_CASE("decomposing a composed transform returns the authored values") {
  const LevelTransform authored = {{1, 2, 3}, {20, -135, 40}, {1, 2.5, 0.5}};
  const std::optional<LevelTransform> result =
      DecomposeMatrix(LocalMatrix(authored));
  REQUIRE(result.has_value());
  CheckVector(result->position, authored.position);
  CheckVector(result->rotation, authored.rotation);
  CheckVector(result->scale, authored.scale);
}

TEST_CASE("a rotated child of a non-uniformly scaled parent is sheared") {
  Level level;
  level.objects.push_back(Object("parent", {{0, 0, 0}, {0, 0, 0}, {3, 1, 1}}));
  level.objects.push_back(
      Object("child", {{0, 0, 0}, {0, 45, 0}, {1, 1, 1}}, "parent"));
  CHECK_FALSE(LevelIndex(level).worldTransform("child").has_value());
}

TEST_CASE("a camera looks along its local -Z with +Y up") {
  Level level;
  LevelObject camera = Object("camera", {{0, 10, 10}, {-45, 0, 0}, {1, 1, 1}});
  camera.kind = LevelKind::Camera;
  level.objects.push_back(camera);

  const Camera3D view = LevelCameraView(LevelIndex(level), camera);
  CHECK(view.position.y == doctest::Approx(10));
  CHECK(view.position.z == doctest::Approx(10));
  const float forwardY = view.target.y - view.position.y;
  const float forwardZ = view.target.z - view.position.z;
  CHECK(forwardY == doctest::Approx(forwardZ));
  CHECK(forwardY < 0);
  CHECK(view.up.y > 0);
  CHECK(view.fovy == doctest::Approx(45));
}
