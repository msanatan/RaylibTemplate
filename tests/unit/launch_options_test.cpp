#include "core/launch_options.h"

#include <doctest/doctest.h>

TEST_CASE("no arguments starts on the splash scene without a script") {
  LaunchOptions options;
  std::string error;
  CHECK(ParseLaunchOptions({}, options, error));
  CHECK(options.scene == "splash");
  CHECK(options.script.empty());
}

TEST_CASE("options are read in any order") {
  LaunchOptions options;
  std::string error;
  CHECK(ParseLaunchOptions({"--script", "run.txt", "--session", "save.txt",
                            "--log", "game.log", "--scene", "gameplay,pause"},
                           options, error));
  CHECK(options.scene == "gameplay,pause");
  CHECK(options.script == "run.txt");
  CHECK(options.session == "save.txt");
  CHECK(options.log == "game.log");
}

TEST_CASE("an unknown option is rejected") {
  LaunchOptions options;
  std::string error;
  CHECK_FALSE(ParseLaunchOptions({"--scrpit", "run.txt"}, options, error));
  CHECK(error == "Unknown option --scrpit");
}

TEST_CASE("an option without a value is rejected") {
  LaunchOptions options;
  std::string error;
  CHECK_FALSE(ParseLaunchOptions({"--scene"}, options, error));
  CHECK(error == "Missing value for --scene");
}
