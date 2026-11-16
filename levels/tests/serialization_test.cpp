#include "levels/document/serialization.h"

#include <doctest/doctest.h>

#include "levels/document/validation.h"

namespace {
const char* const unordered = R"({
  "objects": [
    {"kind": "box", "id": "b", "name": "B", "order": 1,
     "properties": {"zeta": 1, "alpha": {"y": [3, 1], "x": true}, "mystery": {"nested": null}}},
    {"id": "a", "order": 0, "name": "A", "kind": "group"}
  ],
  "version": 1
})";
}  // namespace

TEST_CASE("saving is deterministic and independent of the input's order") {
  const LevelParseResult parsed = ParseLevel(unordered);
  REQUIRE(parsed.level.has_value());
  const std::string text = SerializeLevel(*parsed.level);

  CHECK(text.find("\"id\": \"a\"") < text.find("\"id\": \"b\""));
  CHECK(text.find("\"alpha\"") < text.find("\"zeta\""));
  CHECK(text.back() == '\n');

  const LevelParseResult reread = ParseLevel(text);
  REQUIRE(reread.level.has_value());
  CHECK(SerializeLevel(*reread.level) == text);
}

TEST_CASE("unfamiliar game properties survive a round trip") {
  const LevelParseResult parsed = ParseLevel(unordered);
  REQUIRE(parsed.level.has_value());
  const LevelParseResult reread = ParseLevel(SerializeLevel(*parsed.level));
  REQUIRE(reread.level.has_value());
  CHECK(reread.level->find("b")->properties["mystery"]["nested"].is_null());
  CHECK(reread.level->find("b")->properties["alpha"]["x"] == true);
  CHECK(reread.level->find("b")->properties["alpha"]["y"] ==
        LevelJson::array({3, 1}));
}

TEST_CASE("an unsupported version is rejected") {
  const LevelParseResult parsed =
      ParseLevel(R"({"version": 2, "objects": []})");
  CHECK_FALSE(parsed.level.has_value());
  REQUIRE(parsed.errors.size() == 1);
  CHECK(parsed.errors[0].code == "unsupported_version");
}

TEST_CASE("a syntax error reports its line and column") {
  const LevelParseResult parsed = ParseLevel("{\n  \"version\": 1,\n  oops\n}");
  REQUIRE(parsed.errors.size() == 1);
  CHECK(parsed.errors[0].code == "json_syntax");
  CHECK(parsed.errors[0].line == 3);
  CHECK(parsed.errors[0].column == 3);
}

TEST_CASE("validation reports broken relationships without repairing them") {
  const LevelParseResult parsed =
      ParseLevel(R"({"version": 1, "camera": "a", "objects": [
    {"id": "a", "name": "A", "kind": "group", "order": 0, "parent": "b"},
    {"id": "b", "name": "B", "kind": "group", "order": 0, "parent": "a"},
    {"id": "c", "name": "C", "kind": "box", "order": 3, "parent": "missing",
     "transform": {"scale": [0, 1, 1]}, "properties": {"target": {"$ref": "gone"}}}
  ]})");
  REQUIRE(parsed.level.has_value());

  std::vector<std::string> codes;
  for (const LevelDiagnostic& error : ValidateLevel(*parsed.level)) {
    codes.push_back(error.code);
  }
  for (const char* expected :
       {"hierarchy_cycle", "unknown_parent", "invalid_scale",
        "unknown_reference", "invalid_sibling_order", "invalid_level_camera"}) {
    CAPTURE(expected);
    CHECK(std::find(codes.begin(), codes.end(), expected) != codes.end());
  }
}
