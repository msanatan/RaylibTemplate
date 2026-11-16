#include <doctest/doctest.h>

#include "levels/document/level_index.h"
#include "levels/document/serialization.h"
#include "levels/tests/test_ids.h"
#include "support.h"

namespace {
CliResult Apply(const std::string& level, const std::string& operations,
                std::vector<std::string> extra = {}) {
  std::vector<std::string> arguments = {
      "--file",
      level,
      "apply",
      "--ops",
      "-",
      "--expect",
      Revision(Cli({"--file", level, "inspect"}))};
  arguments.insert(arguments.end(), extra.begin(), extra.end());
  return Cli(arguments, WithTestIds(R"({"operations": [)" + operations + "]}"));
}

Level Reopen(const std::string& level) {
  return *ParseLevel(FileBytes(level)).level;
}

const char* const workshop = R"(
  {"op": "create", "id": "@room", "name": "Room", "kind": "group",
   "transform": {"position": [10, 0, 0], "rotation": [0, 90, 0]}},
  {"op": "create", "id": "@door", "name": "Door", "kind": "box", "parent": "@room",
   "transform": {"position": [0, 1, -2]}, "shape": {"size": [1, 2, 0.2]},
   "properties": {"door": {"locked": true, "opens": {"$ref": "@switch"}}, "tags": ["a", "b"]}},
  {"op": "create", "id": "@switch", "name": "Switch", "kind": "marker", "parent": "@room"},
  {"op": "create", "id": "@lamp", "name": "Lamp", "kind": "sphere", "index": 0}
)";
}  // namespace

TEST_CASE("a hierarchy built through the CLI survives save and reopen") {
  const TemporaryProject project;
  const std::string level = project.level();
  CHECK(Cli({"--file", level, "level new"}).exitCode == 0);

  const CliResult created = Apply(level, workshop);
  REQUIRE(created.exitCode == 0);
  CHECK(created.json["completed"] == "file_commit");
  CHECK(created.json["affected_ids"].size() == 4);

  const CliResult edited = Apply(level, R"(
    {"op": "rename", "id": "@door", "name": "Workshop Door"},
    {"op": "set-transform", "id": "@door", "position": [2, 1, 0]},
    {"op": "set-property", "id": "@door", "path": "/door/locked", "value": false},
    {"op": "reorder", "id": "@switch", "index": 0},
    {"op": "duplicate", "id": "@room", "ids": {"@room": "@room2", "@door": "@door2", "@switch": "@switch2"}},
    {"op": "reparent", "id": "@lamp", "parent": "@room", "mode": "keep-world"}
  )");
  REQUIRE(edited.exitCode == 0);

  const Level saved = Reopen(level);
  const LevelObject& door = *saved.find(TestId("door"));
  CHECK(door.name == "Workshop Door");
  CHECK(door.transform.position.x == 2);
  CHECK(door.transform.scale.x == 1);
  CHECK(door.properties["door"]["locked"] == false);
  CHECK(door.properties["tags"] == LevelJson::array({"a", "b"}));

  const LevelIndex index(saved);
  CHECK(index.children(TestId("room")).front()->id == TestId("switch"));
  CHECK(saved.find(TestId("door"))->order == 1);

  CHECK(saved.find(TestId("door2"))->parent == TestId("room2"));
  CHECK(saved.find(TestId("door2"))->properties["door"]["opens"]["$ref"] ==
        TestId("switch2"));
  CHECK(saved.find(TestId("room2"))->order ==
        saved.find(TestId("room"))->order + 1);

  const std::optional<LevelTransform> lamp =
      index.worldTransform(TestId("lamp"));
  REQUIRE(lamp.has_value());
  CHECK(lamp->position.x == doctest::Approx(0).epsilon(1e-9));
  CHECK(saved.find(TestId("lamp"))->transform.position.z ==
        doctest::Approx(-10));

  CHECK(Cli({"--file", level, "format", "--check"}).exitCode == 0);
  CHECK(SerializeLevel(saved) == FileBytes(level));
}

TEST_CASE("an invalid batch changes nothing") {
  const TemporaryProject project;
  const std::string level = project.level();
  Cli({"--file", level, "level new"});
  Apply(level, workshop);
  const std::string before = FileBytes(level);

  const CliResult result = Apply(level, R"(
    {"op": "rename", "id": "@door", "name": "Changed"},
    {"op": "delete", "id": "@switch"}
  )");
  CHECK(result.exitCode == 2);
  CHECK(result.json["ok"] == false);
  CHECK(result.json["errors"][0]["code"] == "unknown_reference");
  CHECK(result.json["errors"][0]["object_id"] == TestId("door"));
  CHECK(FileBytes(level) == before);

  const CliResult children = Apply(level, R"({"op": "delete", "id": "@room"})");
  CHECK(children.json["errors"][0]["code"] == "has_children");
  CHECK(children.json["errors"][0]["operation"] == 0);
  CHECK(FileBytes(level) == before);
}

TEST_CASE("stale revisions, no-ops, and dry runs never write") {
  const TemporaryProject project;
  const std::string level = project.level();
  Cli({"--file", level, "level new"});
  Apply(level, workshop);
  const std::string before = FileBytes(level);

  const CliResult stale = Cli({"--file", level, "apply", "--ops", "-",
                               "--expect", "file:0000000000000000"},
                              R"({"operations": []})");
  CHECK(stale.exitCode == 3);
  CHECK(stale.json["code"] == "revision_conflict");

  const CliResult noop =
      Apply(level, R"({"op": "rename", "id": "@door", "name": "Door"})");
  CHECK(noop.exitCode == 0);
  CHECK(noop.json["changed"] == false);
  CHECK(noop.json["completed"].is_null());

  const CliResult dryRun =
      Apply(level, R"({"op": "rename", "id": "@door", "name": "Other"})",
            {"--dry-run"});
  CHECK(dryRun.json["changed"] == false);
  CHECK(dryRun.json["would_change"] == true);
  CHECK(dryRun.json["changes"][0]["fields"][0]["path"] == "/name");
  CHECK(FileBytes(level) == before);
}

TEST_CASE("reparenting that would need shear is rejected") {
  const TemporaryProject project;
  const std::string level = project.level();
  Cli({"--file", level, "level new"});
  const CliResult result = Apply(level, R"(
    {"op": "create", "id": "@wide", "name": "Wide", "kind": "group",
     "transform": {"scale": [3, 1, 1]}},
    {"op": "create", "id": "@turned", "name": "Turned", "kind": "box",
     "transform": {"rotation": [0, 45, 0]}},
    {"op": "reparent", "id": "@turned", "parent": "@wide", "mode": "keep-world"}
  )");
  CHECK(result.exitCode == 2);
  CHECK(result.json["errors"][0]["code"] == "unsupported_shear");
  CHECK(result.json["errors"][0]["operation"] == 2);
}

TEST_CASE("unsupported and malformed files are reported, never overwritten") {
  const TemporaryProject project;
  const std::string level = project.level();
  const std::string future = R"({"version": 99, "objects": []})";
  std::ofstream(level) << future;

  const CliResult validated = Cli({"--file", level, "validate"});
  CHECK(validated.exitCode == 2);
  CHECK(validated.json["diagnostics"][0]["code"] == "unsupported_version");
  CHECK(Apply(level, "").exitCode == 2);
  CHECK(Cli({"--file", level, "level new"}).exitCode == 4);
  CHECK(FileBytes(level) == future);

  std::ofstream(level) << "{\n  \"version\": 1,\n  \"objects\": [,]\n}";
  const CliResult syntax = Cli({"--file", level, "validate"});
  CHECK(syntax.json["diagnostics"][0]["code"] == "json_syntax");
  CHECK(syntax.json["diagnostics"][0]["line"] == 3);
}

TEST_CASE("format reports and repairs noncanonical files") {
  const TemporaryProject project;
  const std::string level = project.level();
  std::ofstream(level) << R"({"objects": [], "version": 1})";

  CHECK(Cli({"--file", level, "format", "--check"}).exitCode == 5);
  const std::string revision = Revision(Cli({"--file", level, "inspect"}));
  const CliResult formatted =
      Cli({"--file", level, "format", "--expect", revision});
  CHECK(formatted.exitCode == 0);
  CHECK(formatted.json["changed"] == true);
  CHECK(Cli({"--file", level, "format", "--check"}).exitCode == 0);
}

TEST_CASE("object pages continue with a cursor and reject a stale one") {
  const TemporaryProject project;
  const std::string level = project.level();
  Cli({"--file", level, "level new"});
  Apply(level, workshop);

  const CliResult first =
      Cli({"--file", level, "objects list", "--limit", "3"});
  CHECK(first.json["objects"].size() == 3);
  CHECK(first.json["total"] == 4);
  CHECK(first.json["has_more"] == true);
  CHECK(first.json["objects"][0]["id"] == TestId("lamp"));
  const std::string cursor = first.json["next_cursor"].get<std::string>();

  const CliResult second = Cli(
      {"--file", level, "objects list", "--limit", "3", "--cursor", cursor});
  CHECK(second.json["objects"].size() == 1);
  CHECK(second.json["has_more"] == false);

  CHECK(Cli({"--file", level, "objects list", "--kind", "box", "--cursor",
             cursor})
            .json["code"] == "stale_cursor");
  Apply(level, R"({"op": "rename", "id": "@lamp", "name": "Light"})");
  CHECK(
      Cli({"--file", level, "objects list", "--limit", "3", "--cursor", cursor})
          .exitCode == 3);
}

TEST_CASE("inspection is focused and discovery describes commands") {
  const TemporaryProject project;
  const std::string level = project.level();
  Cli({"--file", level, "level new"});
  Apply(level, workshop);

  const CliResult door =
      Cli({"--file", level, "inspect", "--id", TestId("door")});
  CHECK(door.json["world_transform"]["position"][0] == doctest::Approx(8));
  CHECK(door.json["child_count"] == 0);
  const CliResult locked = Cli({"--file", level, "inspect", "--id",
                                TestId("door"), "--path", "/door/locked"});
  CHECK(locked.json["value"] == true);

  const CliResult described = Cli({"commands", "describe", "apply"});
  CHECK(described.exitCode == 0);
  CHECK(described.json["describe"]["arguments"]["properties"].contains("ops"));
  CHECK(described.json["describe"]["result"]["common"].contains("exit_code"));
  CHECK(described.json["describe"]["result"]["payload"].is_string());
  CHECK(Cli({"--file", level, "save", "--expect", "x"}).json["code"] ==
        "unsupported_target");
  CHECK(Cli({"inspect"}).json["code"] == "missing_target");
}

TEST_CASE("object IDs are generated UUIDs, and readable IDs are rejected") {
  const TemporaryProject project;
  const std::string level = project.level();
  Cli({"--file", level, "level new"});

  const CliResult created =
      Apply(level, R"({"op": "create", "name": "Crate", "kind": "box"})");
  REQUIRE(created.exitCode == 0);
  const std::string id =
      created.json["generated_ids"][0]["id"].get<std::string>();
  CHECK(IsLevelId(id));
  CHECK(created.json["affected_ids"] == LevelJson::array({id}));

  const CliResult copied =
      Apply(level, R"({"op": "duplicate", "id": ")" + id + R"("})");
  REQUIRE(copied.exitCode == 0);
  const std::string copy =
      copied.json["generated_ids"][0]["ids"][id].get<std::string>();
  CHECK(IsLevelId(copy));
  CHECK(copy != id);

  const CliResult readable =
      Apply(level,
            R"({"op": "create", "id": "door", "name": "Door", "kind": "box"})");
  CHECK(readable.exitCode == 2);
  CHECK(readable.json["errors"][0]["code"] == "invalid_id");

  // A hand-edited file with a readable ID fails validation
  std::ofstream(level) << R"({"version": 1, "objects": [
    {"id": "door", "name": "Door", "kind": "box", "order": 0}]})";
  CHECK(Cli({"--file", level, "validate"}).json["diagnostics"][0]["code"] ==
        "invalid_id");
}

TEST_CASE("malformed duplication IDs return diagnostics without writing") {
  const TemporaryProject project;
  const std::string level = project.level();
  REQUIRE(Cli({"--file", level, "level new"}).exitCode == 0);
  REQUIRE(Apply(level, workshop).exitCode == 0);
  const std::string before = FileBytes(level);
  const CliResult result = Apply(
      level, R"({"op": "duplicate", "id": "@door", "ids": {"@door": 123}})");
  CHECK(result.exitCode == 2);
  CHECK(result.json["errors"][0]["code"] == "invalid_argument");
  CHECK(result.json["errors"][0]["path"] == "/ids/" + TestId("door"));
  CHECK(result.json["errors"][0]["operation"] == 0);
  CHECK(FileBytes(level) == before);
}

TEST_CASE("property edits and inspection share JSON Pointer syntax") {
  const TemporaryProject project;
  const std::string level = project.level();
  REQUIRE(Cli({"--file", level, "level new"}).exitCode == 0);
  REQUIRE(Apply(level, workshop).exitCode == 0);
  const std::string before = FileBytes(level);
  for (const std::string path : {"/bad~2key", "/bad~"}) {
    for (const std::string op : {"set-property", "remove-property"}) {
      LevelJson edit = {{"op", op}, {"id", TestId("door")}, {"path", path}};
      if (op == "set-property") edit["value"] = true;
      const CliResult result = Apply(level, edit.dump());
      CHECK(result.exitCode == 2);
      CHECK(result.json["errors"][0]["code"] == "invalid_path");
    }
  }
  CHECK(FileBytes(level) == before);
  REQUIRE(Apply(level, R"({"op": "set-property", "id": "@door",
    "path": "/a~1b/~01/", "value": 42})")
              .exitCode == 0);
  const CliResult inspected = Cli({"--file", level, "inspect", "--id",
                                   TestId("door"), "--path", "/a~1b/~01/"});
  CHECK(inspected.json["value"] == 42);
  CHECK(Apply(level, R"({"op": "set-property", "id": "@door",
    "path": "/tags/01", "value": 42})")
            .exitCode == 2);
}

TEST_CASE("CLI options follow the command and use their declared types") {
  CommandRegistry registry;
  RegisterDocumentCommands(registry);
  RegisterFileCommands(registry);
  const CliArguments apply =
      ParseCliArguments({"--file", "level.json", "apply", "--dry-run", "--ops",
                         "-", "--expect", "file:1"},
                        registry);
  CHECK(apply.file == "level.json");
  CHECK(apply.words == std::vector<std::string>{"apply"});
  CHECK(apply.args["dry-run"] == true);
  CHECK(apply.args["ops"] == "-");
  CHECK(ParseCliArguments(
            {"--file", "level.json", "format", "--check", "false"}, registry)
            .args["check"] == false);
  CHECK(ParseCliArguments({"commands", "describe", "apply"}, registry).words ==
        std::vector<std::string>{"commands", "describe", "apply"});
  CHECK(ParseCliArguments(
            {"--file", "level.json", "objects", "list", "--limit", "-1"},
            registry)
            .args["limit"] == "-1");
  CHECK(Cli({"--dry-run", "apply"}).json["code"] == "invalid_argument");
  CHECK(Cli({"format", "--file", "level.json"}).json["code"] ==
        "invalid_argument");
  CHECK(Cli({"--file", "level.json", "format", "--check", "extra"})
            .json["code"] == "invalid_argument");
  CHECK(Cli({"--file", "level.json", "apply", "--ops"}).json["code"] ==
        "missing_argument");
  CHECK(Cli({"--file", "level.json", "apply", "--ops", "--dry-run"})
            .json["code"] == "missing_argument");
  CHECK(Cli({"--file", "level.json", "inspect", "--unknown"}).json["code"] ==
        "unknown_argument");
}
