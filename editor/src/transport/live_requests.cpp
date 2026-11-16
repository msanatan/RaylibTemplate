#include "transport/live_requests.h"

#include <algorithm>

#include "commands/editor_log.h"

namespace {
LevelJson Run(EditorSession& session, const LevelJson& request) {
  const bool wellFormed =
      request.contains("words") && request["words"].is_array() &&
      std::all_of(request["words"].begin(), request["words"].end(),
                  [](const LevelJson& word) { return word.is_string(); });
  if (!wellFormed) {
    return FailureResult({CommandExit::Invalid, "invalid_request",
                          "A request needs a \"words\" array of strings"});
  }
  CommandRequest command;
  command.words = request["words"].get<std::vector<std::string>>();
  command.args = request.value("args", LevelJson::object());
  if (request.contains("request_id") && request["request_id"].is_string()) {
    command.requestId = request["request_id"].get<std::string>();
  }
  return session.dispatch(command);
}
}  // namespace

bool LiveRequests::start(EditorSession& session, std::string& error) {
  if (!server.start(session.project.root, error)) {
    return false;
  }
  session.sessionId = server.sessionId();
  return true;
}

void LiveRequests::process(EditorSession& session) {
  while (!deferred.has_value()) {
    const std::optional<LiveRequest> live = server.next();
    if (!live.has_value()) {
      return;
    }
    LevelJson result;
    try {
      result = Run(session, live->request);
    } catch (const LevelJson::exception& error) {
      SetLogRequestId("");
      result = FailureResult(
          {CommandExit::Invalid, "invalid_request", error.what()});
    }
    if (result.contains("completed") &&
        result["completed"] == "rendered_capture") {
      deferred = live;
      deferredResult = result;
    } else {
      live->response->set_value(result.dump());
    }
  }
}

void LiveRequests::finishFrame(bool succeeded, const std::string& error) {
  if (!deferred.has_value()) {
    return;
  }
  if (!succeeded) {
    deferredResult.update(
        FailureResult({CommandExit::Io, "write_failed", error}));
  }
  deferred->response->set_value(deferredResult.dump());
  deferred.reset();
}
