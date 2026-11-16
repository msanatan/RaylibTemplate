#include "game/world.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "levels/document/serialization.h"
#include "levels/runtime/loading.h"
#include "levels/tests/test_ids.h"

namespace {
Camera3D FallbackCamera() {
  Camera3D camera = {};
  camera.position = {1, 2, 3};
  camera.fovy = 60;
  return camera;
}

Level AuthoredLevel(double playerX, double cameraHeight) {
  const LevelParseResult parsed =
      ParseLevel(WithTestIds(R"({"version": 1, "camera": "@camera", "objects": [
    {"id": "@player", "name": "Player", "kind": "box", "order": 0,
     "shape": {"size": [2, 2, 2]}, "properties": {"role": "player"}},
    {"id": "@camera", "name": "Camera", "kind": "camera", "order": 1,
     "camera": {"fov": 45}},
    {"id": "@spare", "name": "Spare", "kind": "camera", "order": 2,
     "transform": {"position": [50, 50, 50]}}
  ]})"));
  Level level = *parsed.level;
  level.find(TestId("player"))->transform.position.x = playerX;
  level.find(TestId("camera"))->transform.position.y = cameraHeight;
  return level;
}

void WriteFile(const std::filesystem::path& file, const std::string& text) {
  std::ofstream(file) << text;
}
}  // namespace

TEST_CASE("the designated camera supplies the initial view") {
  const World world(AuthoredLevel(0, 10), FallbackCamera());
  CHECK(world.camera().position.y == doctest::Approx(10));
  CHECK(world.camera().fovy == doctest::Approx(45));
}

TEST_CASE("a level without a designated camera uses the game's own") {
  Level level = AuthoredLevel(0, 10);
  level.camera.reset();
  const World world(level, FallbackCamera());
  CHECK(world.camera().position.x == doctest::Approx(1));
  CHECK(world.camera().fovy == doctest::Approx(60));
  CHECK_FALSE(world.lens().has_value());
}

TEST_CASE("a reload keeps gameplay progress but takes authored edits") {
  World first(AuthoredLevel(0, 10), FallbackCamera());
  first.movePlayer({5, 0, 0});
  const std::string progress = first.saveState();

  // The level is edited and saved while the game runs
  World reloaded(AuthoredLevel(-3, 20), FallbackCamera());
  reloaded.restoreState(progress);
  CHECK(reloaded.player().position.x == doctest::Approx(5));
  CHECK(reloaded.level().find(TestId("player"))->transform.position.x ==
        doctest::Approx(5));
  // The camera is still authored-controlled, so its edit shows up
  CHECK(reloaded.camera().position.y == doctest::Approx(20));
}

TEST_CASE("a reset starts from the authored state") {
  World played(AuthoredLevel(0, 10), FallbackCamera());
  played.movePlayer({5, 0, 0});

  // Reset Level and --fresh-level build the world without restoring state
  const World reset(AuthoredLevel(-3, 10), FallbackCamera());
  CHECK(reset.player().position.x == doctest::Approx(-3));
}

TEST_CASE("a camera the game has taken over survives a reload") {
  World first(AuthoredLevel(0, 10), FallbackCamera());
  Camera3D chase = first.camera();
  chase.position = {7, 8, 9};
  first.controlCamera(chase);

  World reloaded(AuthoredLevel(0, 20), FallbackCamera());
  reloaded.restoreState(first.saveState());
  CHECK(reloaded.isCameraGameControlled());
  CHECK(reloaded.camera().position.x == doctest::Approx(7));
  CHECK(reloaded.camera().position.y == doctest::Approx(8));
}

TEST_CASE("an invalid saved level keeps the last valid one loaded") {
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() / "gbm_loading_test.json";
  WriteFile(file, SerializeLevel(AuthoredLevel(1, 10)));
  CHECK(LoadGameLevel(file.string()).current);

  WriteFile(file, R"({"version": 1, "camera": "nobody", "objects": []})");
  const LoadedLevel rejected = LoadGameLevel(file.string());
  CHECK_FALSE(rejected.current);
  CHECK(rejected.level.find(TestId("player"))->transform.position.x ==
        doctest::Approx(1));

  WriteFile(file, SerializeLevel(AuthoredLevel(4, 10)));
  const LoadedLevel edited = LoadGameLevel(file.string());
  CHECK(edited.current);
  CHECK(edited.level.find(TestId("player"))->transform.position.x ==
        doctest::Approx(4));
  std::filesystem::remove(file);
}

TEST_CASE("player movement refreshes descendants under a transformed parent") {
  Level level = AuthoredLevel(1, 10);
  LevelObject parent;
  parent.id = TestId("parent");
  parent.transform = {{5, 0, 0}, {0, 90, 0}, {2, 2, 2}};
  LevelObject child;
  child.id = TestId("child");
  child.parent = TestId("player");
  child.transform.position = {0, 1, 0};
  level.find(TestId("player"))->parent = parent.id;
  level.objects.push_back(parent);
  level.objects.push_back(child);
  World world(level, FallbackCamera());
  const auto* children = &world.level().children(TestId("player"));
  const auto* player = world.level().find(TestId("player"));
  world.movePlayer({3, 0, 0});
  CHECK(world.player().position.x == doctest::Approx(8));
  CHECK(world.level().world(TestId("player")).at(0, 3) == doctest::Approx(8));
  CHECK(world.level().world(TestId("child")).at(0, 3) == doctest::Approx(8));
  CHECK(world.level().world(TestId("child")).at(1, 3) == doctest::Approx(2));
  CHECK(world.level().world(TestId("camera")).at(1, 3) == doctest::Approx(10));
  CHECK(&world.level().children(TestId("player")) == children);
  CHECK(world.level().find(TestId("player")) == player);
  world.movePlayer({0, 0, 0});
  CHECK(world.level().world(TestId("child")).at(0, 3) == doctest::Approx(8));
}
