#include "document/document.h"

#include <doctest/doctest.h>

#include "document/session.h"
#include "levels/document/serialization.h"
#include "levels/editing/file_target.h"
#include "levels/tests/test_ids.h"
#include "support.h"

namespace {
Project ProjectAt(const TemporaryProject& temporary) {
  Project project;
  project.root = temporary.root.string();
  project.assetsDirectory = (temporary.root / "assets").string();
  return project;
}

LevelJson CreateBox(const std::string& name) {
  return {
      {"op", "create"}, {"id", TestId(name)}, {"name", name}, {"kind", "box"}};
}

LevelJson Move(const std::string& name, double x) {
  return {
      {"op", "set-transform"}, {"id", TestId(name)}, {"position", {x, 0, 0}}};
}

double X(const EditorSession& session, const std::string& name) {
  return session.doc().level().find(TestId(name))->transform.position.x;
}

void WriteLevel(const std::string& path, const Level& level) {
  std::ofstream(path) << SerializeLevel(level);
}
}  // namespace

TEST_CASE("editor edits and command batches share one history") {
  const TemporaryProject temporary;
  EditorSession session(ProjectAt(temporary));
  session.newLevel(temporary.level());

  REQUIRE(session.edit({CreateBox("crate")}, "Create"));
  const LevelJson batch = {
      {"operations", {CreateBox("barrel"), Move("crate", 4)}}};
  const LevelJson applied =
      session.dispatch({{"apply"},
                        {{"ops", batch},
                         {"expect", session.doc().revision()},
                         {"document", session.doc().id()}},
                        ""});
  REQUIRE(applied["ok"] == true);
  CHECK(applied["completed"] == "live_transaction");
  CHECK(session.doc().history().undoCount() == 2);

  CHECK(session.undo());
  CHECK(session.doc().level().find(TestId("barrel")) == nullptr);
  CHECK(X(session, "crate") == 0);
  CHECK(session.undo());
  CHECK(session.doc().level().objects.empty());
  CHECK(session.redo());
  CHECK(session.redo());
  CHECK(X(session, "crate") == 4);
}

TEST_CASE("a drag is one undo step, and cancelling leaves no trace") {
  const TemporaryProject temporary;
  EditorSession session(ProjectAt(temporary));
  session.newLevel(temporary.level());
  session.edit({CreateBox("crate")}, "Create");
  const std::string revision = session.doc().revision();

  session.beginGesture("drag");
  for (const double x : {1.0, 2.0, 3.0}) {
    CHECK(session.previewEdit("drag", {Move("crate", x)}));
  }
  CHECK_FALSE(session.previewEdit("drag", {{{"op", "set-transform"},
                                            {"id", TestId("crate")},
                                            {"scale", {0, 1, 1}}}}));

  const LevelJson during = session.dispatch(
      {{"undo"}, {{"expect", revision}, {"document", session.doc().id()}}, ""});
  CHECK(during["code"] == "gesture_in_progress");

  session.finishGesture("drag", "Move");
  CHECK(X(session, "crate") == 3);
  CHECK(session.doc().history().undoCount() == 2);
  session.undo();
  CHECK(X(session, "crate") == 0);
  session.redo();

  const std::string beforeCancel = session.doc().revision();
  session.beginGesture("drag");
  session.previewEdit("drag", {Move("crate", 9)});
  session.cancelGesture("drag");
  CHECK(X(session, "crate") == 3);
  CHECK(session.doc().history().undoCount() == 2);
  CHECK(session.doc().revision() == beforeCancel);
}

TEST_CASE("dirty tracking follows the saved state through undo") {
  const TemporaryProject temporary;
  EditorSession session(ProjectAt(temporary));
  session.newLevel(temporary.level());
  CHECK(session.doc().isDirty());
  session.edit({CreateBox("crate")}, "Create");
  session.save();
  CHECK_FALSE(session.doc().isDirty());

  session.edit({Move("crate", 2)}, "Move");
  CHECK(session.doc().isDirty());
  session.undo();
  CHECK_FALSE(session.doc().isDirty());
}

TEST_CASE("only the control that began a gesture can end it") {
  const TemporaryProject temporary;
  EditorSession session(ProjectAt(temporary));
  session.newLevel(temporary.level());
  session.edit({CreateBox("crate")}, "Create");

  session.beginGesture("properties");
  session.previewEdit("properties", {Move("crate", 1)});
  // Another control idling in the same frame must not commit the drag
  session.finishGesture("gizmo", "Move");
  session.cancelGesture("gizmo");
  CHECK_FALSE(session.previewEdit("gizmo", {Move("crate", 9)}));
  CHECK(session.doc().isGestureActive());

  session.previewEdit("properties", {Move("crate", 2)});
  session.finishGesture("properties", "Move");
  CHECK(X(session, "crate") == 2);
  CHECK(session.doc().history().undoCount() == 2);
}

TEST_CASE("undo stops at the oldest retained step") {
  const TemporaryProject temporary;
  EditorSession session(ProjectAt(temporary));
  session.newLevel(temporary.level());
  session.edit({CreateBox("crate")}, "Create");
  for (size_t i = 1; i <= History::maximumSteps + 5; i++) {
    session.edit({Move("crate", (double)i)}, "Move");
  }
  CHECK(session.doc().history().undoCount() == History::maximumSteps);

  size_t undone = 0;
  while (session.undo()) {
    undone++;
  }
  CHECK(undone == History::maximumSteps);
  CHECK_FALSE(session.undo());
  CHECK(X(session, "crate") == 5);
}

TEST_CASE("outside changes reload a clean document and never clobber edits") {
  const TemporaryProject temporary;
  const std::string path = temporary.level();
  EditorSession session(ProjectAt(temporary));
  session.newLevel(path);
  session.edit({CreateBox("crate")}, "Create");
  session.save();
  session.pollDisk();
  CHECK(session.lastReload.empty());

  Level outside = session.doc().level();
  outside.find(TestId("crate"))->transform.position.x = 7;
  WriteLevel(path, outside);
  session.pollDisk();
  CHECK(X(session, "crate") == 7);
  CHECK(session.doc().history().undoCount() == 0);
  CHECK_FALSE(session.doc().isDirty());

  std::ofstream(path) << R"({"version": 1, "camera": "nobody", "objects": []})";
  session.pollDisk();
  CHECK(session.lastReload.find("does not exist") != std::string::npos);
  CHECK(X(session, "crate") == 7);

  session.edit({Move("crate", 1)}, "Move");
  outside.find(TestId("crate"))->transform.position.x = 8;
  WriteLevel(path, outside);
  const std::string onDisk = FileBytes(path);
  session.pollDisk();
  CHECK(session.doc().hasDiskConflict());
  CHECK(X(session, "crate") == 1);
  CHECK_THROWS_AS(session.save(), CommandFailure);
  CHECK(FileBytes(path) == onDisk);

  session.keepLocalChanges();
  session.save();
  CHECK(ParseLevel(FileBytes(path))
            .level->find(TestId("crate"))
            ->transform.position.x == 1);
}

TEST_CASE(
    "command registrations preserve each target's handler and arguments") {
  const TemporaryProject temporary;
  EditorSession session(ProjectAt(temporary));
  const std::string path = temporary.level();
  FileTarget file(path);
  const LevelJson created = session.registry.dispatch(file, {{"level", "new"}});
  REQUIRE(created["ok"] == true);
  CHECK(created["completed"] == "file_commit");
  CHECK_FALSE(session.doc().isOpen());
  const auto fileDescription =
      session.registry.dispatch(file, {{"commands", "describe", "level new"}});
  CHECK(fileDescription["describe"]["arguments"]["required"].empty());
  const auto liveDescription =
      session.dispatch({{"commands", "describe", "level new"}});
  CHECK(liveDescription["describe"]["arguments"]["required"] ==
        LevelJson::array({"path"}));
  CHECK(liveDescription["describe"]["side_effects"] == true);
  CHECK_THROWS_AS(
      session.registry.add(*session.registry.find("level new", "file")),
      std::logic_error);
}

TEST_CASE(
    "opening activates the prepared level and keeps unsaved work on failure") {
  const TemporaryProject temporary;
  EditorSession session(ProjectAt(temporary));
  session.newLevel(temporary.level());
  REQUIRE(session.edit(CreateBox("crate"), "Create"));
  const std::string revision = session.doc().revision();
  const std::string other = temporary.level("other.json");
  std::ofstream(other) << "invalid";
  const auto failed = session.dispatch({{"level", "open"},
                                        {{"path", other},
                                         {"unsaved", "discard"},
                                         {"document", session.doc().id()},
                                         {"expect", revision}}});
  CHECK(failed["ok"] == false);
  CHECK(session.doc().revision() == revision);
  CHECK(session.doc().isDirty());
  WriteLevel(other, Level());
  EditorDocument prepared = session.prepareOpenLevel(other);
  std::ofstream(other) << "changed after preparation";
  session.activateLevel(std::move(prepared));
  CHECK(session.doc().path() == other);
  CHECK(session.doc().level().objects.empty());
  CHECK_FALSE(session.doc().isDirty());
}

TEST_CASE("live handlers use the session supplied to dispatch") {
  const TemporaryProject temporary;
  EditorSession first(ProjectAt(temporary));
  EditorSession second(ProjectAt(temporary));
  CommandRegistry registry;
  RegisterLiveCommands(registry);
  const std::string firstPath = temporary.level("first.json");
  const std::string secondPath = temporary.level("second.json");
  REQUIRE(registry.dispatch(
              first, {{"level", "new"}, {{"path", firstPath}}})["ok"] == true);
  REQUIRE(registry.dispatch(second, {{"level", "new"},
                                     {{"path", secondPath}}})["ok"] == true);
  CHECK(first.doc().path() == firstPath);
  CHECK(second.doc().path() == secondPath);
}
