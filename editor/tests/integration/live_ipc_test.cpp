#include <doctest/doctest.h>

#include <atomic>
#include <cstdlib>
#include <thread>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include "commands/editor_log.h"
#include "document/session.h"
#include "levels/document/serialization.h"
#include "levels/tests/test_ids.h"
#include "support.h"
#include "transport/command_server.h"
#include "transport/live_requests.h"

namespace {
struct LiveEditor {
  TemporaryProject temporary;
  EditorSession session;
  LiveRequests requests;
  std::atomic<bool> pumping{true};
  std::thread pump;

  LiveEditor() : session(MakeProject()) {
#if defined(_WIN32)
    _putenv_s("SCENIC_HOME", temporary.root.string().c_str());
#else
    setenv("SCENIC_HOME", temporary.root.c_str(), 1);
#endif
    StartEditorLog("");
    std::string error;
    REQUIRE(requests.start(session, error));
    session.newLevel(temporary.level());
    pump = std::thread([this] {
      while (pumping) {
        requests.process(session);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  }
  ~LiveEditor() { stop(); }

  void stop() {
    if (pumping.exchange(false)) {
      pump.join();
      requests.stop();
    }
  }

  Project MakeProject() const {
    Project project;
    project.root = temporary.root.string();
    project.assetsDirectory = (temporary.root / "assets").string();
    return project;
  }

  CliResult run(std::vector<std::string> arguments,
                const std::string& standardInput = "") {
    arguments.insert(arguments.begin(), {"--editor", session.sessionId});
    return Cli(arguments, standardInput);
  }

  CliResult mutate(std::vector<std::string> arguments,
                   const std::string& standardInput = "") {
    const CliResult inspected = run({"inspect"});
    arguments.insert(
        arguments.end(),
        {"--document", inspected.json["document"]["id"].get<std::string>(),
         "--expect", Revision(inspected)});
    return run(arguments, standardInput);
  }
};

const std::string createCrate = WithTestIds(
    R"({"operations": [{"op": "create", "id": "@crate", "name": "Crate", "kind": "box"}]})");
}  // namespace

TEST_CASE("a live edit is discoverable, undoable, and saved only on request") {
  LiveEditor editor;
  const std::string path = editor.temporary.level();

  const CliResult sessions = Cli({"sessions"});
  REQUIRE(sessions.json["sessions"].size() == 1);
  CHECK(sessions.json["sessions"][0]["id"] == editor.session.sessionId);

  const CliResult applied = editor.mutate({"apply", "--ops", "-"}, createCrate);
  REQUIRE(applied.exitCode == 0);
  CHECK(applied.json["completed"] == "live_transaction");
  CHECK(applied.json["target"]["session"] == editor.session.sessionId);
  CHECK(applied.json["document"]["dirty"] == true);
  CHECK_FALSE(std::filesystem::exists(path));

  const CliResult saved = editor.mutate({"save"});
  CHECK(saved.json["completed"] == "save");
  CHECK(saved.json["document"]["dirty"] == false);
  CHECK(ParseLevel(FileBytes(path)).level->find(TestId("crate")) != nullptr);

  const CliResult undone = editor.mutate({"undo"});
  CHECK(undone.json["changed"] == true);
  CHECK(undone.json["affected_ids"] == LevelJson::array({TestId("crate")}));
  CHECK(editor.run({"inspect"}).json["summary"]["object_count"] == 0);
}

TEST_CASE("live mutations are bound to a document and a revision") {
  LiveEditor editor;
  const CliResult inspected = editor.run({"inspect"});
  const std::string document =
      inspected.json["document"]["id"].get<std::string>();

  const CliResult wrongDocument =
      editor.run({"apply", "--ops", "-", "--document", "another-document",
                  "--expect", Revision(inspected)},
                 createCrate);
  CHECK(wrongDocument.exitCode == 3);
  CHECK(wrongDocument.json["code"] == "document_mismatch");

  const CliResult stale = editor.run(
      {"apply", "--ops", "-", "--document", document, "--expect", "live:99"},
      createCrate);
  CHECK(stale.exitCode == 3);
  CHECK(stale.json["code"] == "revision_conflict");

  const CliResult unbound = editor.run(
      {"apply", "--ops", "-", "--expect", Revision(inspected)}, createCrate);
  CHECK(unbound.json["code"] == "missing_argument");
  CHECK(editor.run({"inspect"}).json["summary"]["object_count"] == 0);
}

TEST_CASE("retrying a request does not apply it twice") {
  LiveEditor editor;
  const CliResult inspected = editor.run({"inspect"});
  const std::vector<std::string> request = {
      "apply",
      "--ops",
      "-",
      "--document",
      inspected.json["document"]["id"].get<std::string>(),
      "--expect",
      Revision(inspected),
      "--request-id",
      "retry-1"};

  const CliResult first = editor.run(request, createCrate);
  REQUIRE(first.exitCode == 0);
  // The same request again, as after a lost response: its revision is now
  // stale, but the first result is returned rather than a conflict
  const CliResult retried = editor.run(request, createCrate);
  CHECK(retried.exitCode == 0);
  CHECK(retried.json["deduplicated"] == true);
  CHECK(Revision(retried) == Revision(first));
  CHECK(editor.session.doc().history().undoCount() == 1);

  const CliResult reused = editor.mutate({"undo", "--request-id", "retry-1"});
  CHECK(reused.json["code"] == "request_id_reused");
}

TEST_CASE("switching levels never drops unsaved work silently") {
  LiveEditor editor;
  editor.mutate({"apply", "--ops", "-"}, createCrate);
  const std::string other = editor.temporary.level("other.json");

  const CliResult refused = editor.run({"level new", "--path", other});
  CHECK(refused.exitCode == 3);
  CHECK(refused.json["code"] == "unsaved_changes");

  const CliResult switched =
      editor.mutate({"level new", "--path", other, "--unsaved", "save"});
  REQUIRE(switched.exitCode == 0);
  CHECK(switched.json["document"]["path"] == other);
  CHECK(ParseLevel(FileBytes(editor.temporary.level()))
            .level->find(TestId("crate")) != nullptr);

  const CliResult reopened = editor.run(
      {"level open", "--path", editor.temporary.level(), "--unsaved", "discard",
       "--document", switched.json["document"]["id"].get<std::string>(),
       "--expect", Revision(switched)});
  REQUIRE(reopened.exitCode == 0);
  CHECK(reopened.json["document"]["id"] != switched.json["document"]["id"]);
  CHECK(editor.session.doc().history().undoCount() == 0);
}

TEST_CASE("logs page by sequence and report what a request logged") {
  LiveEditor editor;
  editor.mutate({"apply", "--ops", "-", "--request-id", "logged"}, createCrate);

  const CliResult page =
      editor.run({"logs", "--subsystem", "SCENIC", "--request", "logged"});
  REQUIRE(page.json["records"].size() == 1);
  CHECK(page.json["records"][0]["request_id"] == "logged");
  const std::string cursor = page.json["next_cursor"].get<std::string>();

  CHECK(editor
            .run({"logs", "--subsystem", "SCENIC", "--request", "logged",
                  "--cursor", cursor})
            .json["records"]
            .empty());
  CHECK(editor.run({"logs", "--subsystem", "LEVEL", "--cursor", cursor})
            .json["code"] == "stale_cursor");
}

TEST_CASE("a missing editor is an error, never a file edit") {
  LiveEditor editor;
  editor.mutate({"apply", "--ops", "-"}, createCrate);
  editor.mutate({"save"});
  const std::string path = editor.temporary.level();
  const std::string onDisk = FileBytes(path);
  const std::string session = editor.session.sessionId;
  editor.stop();

  const CliResult result =
      Cli({"--editor", session, "apply", "--ops", "-", "--document", "any",
           "--expect", "live:1"},
          WithTestIds(R"({"operations": [{"op": "delete", "id": "@crate"}]})"));
  CHECK(result.exitCode == 4);
  CHECK(result.json["code"] == "session_unavailable");
  CHECK(FileBytes(path) == onDisk);
  CHECK(Cli({"sessions"}).json["sessions"].empty());
}

#if !defined(_WIN32)
TEST_CASE(
    "a client that disconnects before its answer does not stop the editor") {
  LiveEditor editor;
  const std::optional<SessionInfo> session =
      FindSession(editor.session.sessionId);
  REQUIRE(session.has_value());

  sockaddr_un address = {};
  address.sun_family = AF_UNIX;
  session->socket.copy(address.sun_path, sizeof(address.sun_path) - 1);
  const int client = socket(AF_UNIX, SOCK_STREAM, 0);
  if (client < 0) {
    FAIL("could not create a socket");
    return;
  }
  REQUIRE(connect(client, (const sockaddr*)&address, sizeof(address)) == 0);
  const std::string request = LevelJson{{"words", {"inspect"}}}.dump() + "\n";
  REQUIRE(write(client, request.data(), request.size()) ==
          (ssize_t)request.size());
  close(client);

  // Reaching this line at all means the failed send did not raise SIGPIPE
  CHECK(editor.run({"inspect"}).exitCode == 0);
}
#endif

TEST_CASE("malformed requests leave the live editor and unsaved work intact") {
  LiveEditor editor;
  REQUIRE(editor.mutate({"apply", "--ops", "-"}, createCrate).exitCode == 0);
  const std::optional<SessionInfo> session =
      FindSession(editor.session.sessionId);
  REQUIRE(session.has_value());
  for (const std::string& request :
       {std::string("{"), std::string("null"), std::string("123"),
        std::string(R"({"words": [123]})")}) {
    std::string error;
    const auto response = SendToSession(*session, request, error);
    REQUIRE(response.has_value());
    CHECK(LevelJson::parse(*response)["code"] == "invalid_request");
  }
  const CliResult invalid = editor.mutate(
      {"apply", "--ops", "-"},
      WithTestIds(
          R"({"operations": [{"op": "duplicate", "id": "@crate", "ids": {"@crate": 123}}]})"));
  CHECK(invalid.exitCode == 2);
  CHECK(invalid.json["errors"][0]["code"] == "invalid_argument");
  const CliResult inspected = editor.run({"inspect"});
  CHECK(inspected.exitCode == 0);
  CHECK(inspected.json["summary"]["object_count"] == 1);
  CHECK(inspected.json["document"]["dirty"] == true);
  CHECK(editor.session.doc().history().undoCount() == 1);
}
