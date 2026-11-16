#include "transport/cli.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "document/document.h"
#include "document/session.h"
#include "levels/editing/commands.h"
#include "levels/editing/file_target.h"
#include "transport/command_server.h"

namespace {
int Finish(const LevelJson& result, std::ostream& out) {
  out << result.dump(2) << "\n";
  return CommandExitCode(result);
}

bool IsFlag(const std::string& argument) {
  return argument.size() > 2 && argument.rfind("--", 0) == 0;
}

}  // namespace

CliArguments ParseCliArguments(const std::vector<std::string>& arguments,
                               const CommandRegistry& registry) {
  CliArguments parsed;
  size_t cursor = 0;
  const auto requireValue = [&](const std::string& flag) {
    if (cursor == arguments.size() || IsFlag(arguments[cursor])) {
      throw CommandFailure{CommandExit::Invalid, "missing_argument",
                           "--" + flag + " needs a value"};
    }
    return arguments[cursor++];
  };
  while (cursor < arguments.size() && IsFlag(arguments[cursor])) {
    const std::string flag = arguments[cursor++].substr(2);
    if (flag == "editor") {
      parsed.editor = requireValue(flag);
      parsed.hasEditor = true;
    } else if (flag == "file") {
      parsed.file = requireValue(flag);
      parsed.hasFile = true;
    } else if (flag == "assets") {
      parsed.assets = requireValue(flag);
    } else if (flag == "request-id") {
      parsed.requestId = requireValue(flag);
    } else {
      throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                           "Command options must follow the command"};
    }
  }
  if (parsed.hasEditor && parsed.hasFile) {
    throw CommandFailure{CommandExit::Invalid, "invalid_target",
                         "Use either --editor or --file, not both"};
  }

  const std::string target = parsed.hasEditor ? "editor"
                             : parsed.hasFile ? "file"
                                              : "";
  const Command* command = nullptr;
  std::string name;
  while (cursor < arguments.size() && !IsFlag(arguments[cursor])) {
    const std::string& word = arguments[cursor++];
    parsed.words.push_back(word);
    name += (name.empty() ? "" : " ") + word;
    if (const Command* match = registry.find(name, target)) {
      command = match;
    }
  }
  if (cursor == arguments.size()) {
    return parsed;
  }
  if (command == nullptr) {
    throw CommandFailure{
        CommandExit::Invalid, "unknown_command",
        "Unknown command \"" + name + "\". Run `scenic-cli commands`"};
  }
  while (cursor < arguments.size()) {
    if (!IsFlag(arguments[cursor])) {
      throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                           "Positional arguments must precede command options"};
    }
    const std::string flag = arguments[cursor++].substr(2);
    if (flag == "file" || flag == "editor" || flag == "assets") {
      throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                           "--" + flag + " must precede the command"};
    }
    if (flag == "request-id") {
      parsed.requestId = requireValue(flag);
      continue;
    }
    const auto param = std::find_if(
        command->params.begin(), command->params.end(),
        [&](const CommandParam& candidate) { return candidate.name == flag; });
    if (param == command->params.end() && flag != "expect" &&
        flag != "document") {
      throw CommandFailure{
          CommandExit::Invalid, "unknown_argument",
          "\"" + command->name + "\" has no argument --" + flag};
    }
    if (param != command->params.end() && param->type == "boolean") {
      parsed.args[flag] = true;
      if (cursor < arguments.size() &&
          (arguments[cursor] == "true" || arguments[cursor] == "false")) {
        parsed.args[flag] = arguments[cursor++] == "true";
      }
    } else {
      parsed.args[flag] = requireValue(flag);
    }
  }
  return parsed;
}

namespace {
void LoadOperations(LevelJson& args, std::istream& in) {
  if (!args.contains("ops") || !args["ops"].is_string()) {
    return;
  }
  const std::string source = args["ops"].get<std::string>();
  std::optional<std::string> text;
  if (source == "-") {
    std::ostringstream buffer;
    buffer << in.rdbuf();
    text = buffer.str();
  } else {
    text = ReadTextFile(source);
  }
  if (!text.has_value()) {
    throw CommandFailure{
        CommandExit::Io, "file_not_found",
        "Could not read the operations file \"" + source + "\""};
  }
  args["ops"] = LevelJson::parse(*text, nullptr, false);
  if (args["ops"].is_discarded()) {
    throw CommandFailure{CommandExit::Invalid, "invalid_request",
                         "The operations are not valid JSON"};
  }
}

LevelJson ListSessionsResult() {
  LevelJson sessions = LevelJson::array();
  for (const SessionInfo& session : ListSessions()) {
    sessions.push_back({{"id", session.id},
                        {"project", session.project},
                        {"pid", session.pid}});
  }
  return {
      {"ok", true}, {"code", "ok"}, {"sessions", sessions}, {"exit_code", 0}};
}

LevelJson RunLive(const CliArguments& parsed) {
  const std::optional<SessionInfo> session = FindSession(parsed.editor);
  // A missing editor is an error, never a reason to edit the file instead
  if (!session.has_value()) {
    throw CommandFailure{CommandExit::Io, "session_unavailable",
                         "No running editor session \"" + parsed.editor +
                             "\". Run `scenic-cli sessions` to list them"};
  }
  const LevelJson request = {
      {"words", parsed.words},
      {"args", parsed.args},
      {"request_id",
       parsed.requestId.empty() ? NewLevelId() : parsed.requestId}};
  std::string error;
  const std::optional<std::string> response =
      SendToSession(*session, request.dump(), error);
  if (!response.has_value()) {
    throw CommandFailure{CommandExit::Io, "session_unavailable", error};
  }
  LevelJson result = LevelJson::parse(*response, nullptr, false);
  if (!result.is_object()) {
    throw CommandFailure{CommandExit::Io, "session_unavailable",
                         "The editor sent an unreadable response"};
  }
  return result;
}
}  // namespace

int RunCli(const std::vector<std::string>& arguments, std::istream& in,
           std::ostream& out) {
  CommandRegistry registry;
  RegisterDocumentCommands(registry);
  RegisterFileCommands(registry);
  RegisterLiveCommands(registry);

  try {
    CliArguments parsed = ParseCliArguments(arguments, registry);
    if (parsed.words == std::vector<std::string>{"sessions"}) {
      return Finish(ListSessionsResult(), out);
    }
    const bool discovery =
        !parsed.words.empty() && parsed.words.front() == "commands";
    if (!parsed.hasEditor && !parsed.hasFile && !discovery) {
      throw CommandFailure{
          CommandExit::Invalid, "missing_target",
          "Name a target: --editor <session-id> for a running editor (see "
          "`scenic-cli sessions`) or --file <level> for a level file"};
    }
    if (parsed.words.empty()) {
      throw CommandFailure{CommandExit::Invalid, "unknown_command",
                           "No command given. Run `scenic-cli commands`"};
    }

    LoadOperations(parsed.args, in);
    if (parsed.hasEditor) {
      return Finish(RunLive(parsed), out);
    }
    FileTarget target(parsed.file, parsed.assets);
    return Finish(registry.dispatch(
                      target, {parsed.words, parsed.args, parsed.requestId}),
                  out);
  } catch (const CommandFailure& failure) {
    return Finish(FailureResult(failure), out);
  }
}
