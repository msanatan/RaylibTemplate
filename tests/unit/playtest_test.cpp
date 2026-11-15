#include "core/testing/playtest.h"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {
class QuietScene : public Scene {
 public:
  explicit QuietScene(SceneManager& scenes) : Scene(scenes) {}
  const char* name() const override { return "quiet"; }
  void update(float) override {}
  void draw() const override {}
};

struct PlaytestFixture {
  PlaytestFixture() {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    folder = std::filesystem::temp_directory_path() /
             ("raylib-playtest-" + std::to_string(stamp));
    std::filesystem::create_directory(folder);
    scenes.push<QuietScene>();
    scenes.update(0.0f);
  }

  ~PlaytestFixture() {
    std::error_code error;
    std::filesystem::remove_all(folder, error);
  }

  bool load(const std::string& text) {
    const auto script = folder / "script.txt";
    std::ofstream(script) << text;
    return playtest.load(script.string());
  }

  std::filesystem::path folder;
  SceneManager scenes;
  Playtest playtest;
};
}  // namespace

TEST_CASE_FIXTURE(PlaytestFixture, "expect_exit requires the game to exit") {
  REQUIRE(load("wait 1\nexpect_exit\n"));
  SUBCASE("exit after the preceding command") {
    playtest.beginFrame(scenes);
    CHECK(playtest.finish());
    CHECK(playtest.isFinished());
  }
  SUBCASE("game keeps running") {
    playtest.beginFrame(scenes);
    playtest.beginFrame(scenes);
    CHECK(playtest.hasFailed());
    CHECK_FALSE(playtest.finish());
  }
}

TEST_CASE_FIXTURE(PlaytestFixture,
                  "quit and normal script completion succeed") {
  SUBCASE("quit") { REQUIRE(load("expect quiet\nquit\n")); }
  SUBCASE("end of script") { REQUIRE(load("expect quiet\n")); }
  playtest.beginFrame(scenes);
  CHECK(playtest.isFinished());
  CHECK(playtest.finish());
}

TEST_CASE_FIXTURE(PlaytestFixture,
                  "a failed assertion stays failed when the game exits") {
  REQUIRE(load("expect missing\n"));
  playtest.beginFrame(scenes);
  CHECK_FALSE(playtest.finish());
  CHECK(playtest.hasFailed());
}
