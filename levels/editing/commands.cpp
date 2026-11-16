#include "levels/editing/commands.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace {
const char* ExitCodeName(CommandExit exit) {
  switch (exit) {
    case CommandExit::Success:
      return "ok";
    case CommandExit::Invalid:
      return "invalid";
    case CommandExit::Conflict:
      return "conflict";
    case CommandExit::Io:
      return "io";
    case CommandExit::Noncanonical:
      return "noncanonical";
  }
  return "invalid";
}

std::string JoinWords(const std::vector<std::string>& words, size_t first,
                      size_t count) {
  std::string joined;
  for (size_t i = first; i < first + count; i++) {
    joined += (joined.empty() ? "" : " ") + words[i];
  }
  return joined;
}

LevelJson DocumentJson(const CommandTarget& target) {
  const DocumentState document = target.document();
  if (!document.open) {
    return nullptr;
  }
  LevelJson json = {{"path", document.path},
                    {"revision", document.revision},
                    {"dirty", document.dirty}};
  if (!document.id.empty()) {
    json["id"] = document.id;
  }
  return json;
}

// Command lines deliver every value as text, so values are converted to the
// declared type here, where both adapters benefit
void CoerceArgument(const CommandParam& param, LevelJson& value) {
  const auto invalid = [&](const std::string& expected) {
    throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                         "--" + param.name + " must be " + expected};
  };

  if (param.type == "string") {
    if (!value.is_string()) {
      invalid("a string");
    }
  } else if (param.type == "integer") {
    if (value.is_string()) {
      const std::string text = value.get<std::string>();
      size_t used = 0;
      long long parsed = 0;
      try {
        parsed = std::stoll(text, &used);
      } catch (const std::exception&) {
        used = 0;
      }
      if (text.empty() || used != text.size()) {
        invalid("an integer");
      }
      value = parsed;
    } else if (!value.is_number_integer()) {
      invalid("an integer");
    }
  } else if (param.type == "boolean") {
    if (value == "true" || value == "false") {
      value = value == "true";
    } else if (!value.is_boolean()) {
      invalid("true or false");
    }
  } else if (param.type == "strings") {
    if (value.is_string()) {
      LevelJson list = LevelJson::array();
      std::istringstream parts(value.get<std::string>());
      std::string part;
      while (std::getline(parts, part, ',')) {
        list.push_back(part);
      }
      value = list;
    }
    if (!value.is_array() ||
        !std::all_of(value.begin(), value.end(),
                     [](const LevelJson& item) { return item.is_string(); })) {
      invalid("a comma-separated list");
    }
  }
}

LevelJson PrepareArguments(const Command& command, const LevelJson& supplied,
                           const std::string& positional, bool live) {
  std::vector<CommandParam> params = command.params;
  if (command.mutates) {
    params.push_back({"expect", "string", true, ""});
  }
  if (live) {
    params.push_back({"document", "string", command.mutates, ""});
  }

  LevelJson args = supplied.is_object() ? supplied : LevelJson::object();
  if (!positional.empty()) {
    if (command.positional.empty()) {
      throw CommandFailure{
          CommandExit::Invalid, "invalid_argument",
          "\"" + command.name + "\" takes no argument \"" + positional + "\""};
    }
    args[command.positional] = positional;
  }

  for (auto& [name, value] : args.items()) {
    const auto param =
        std::find_if(params.begin(), params.end(),
                     [&](const CommandParam& p) { return p.name == name; });
    if (param == params.end()) {
      throw CommandFailure{CommandExit::Invalid, "unknown_argument",
                           "\"" + command.name + "\" has no argument --" +
                               name + ". Run `commands describe " +
                               command.name + "` for its arguments"};
    }
    CoerceArgument(*param, value);
  }
  for (const CommandParam& param : params) {
    if (param.required && !args.contains(param.name)) {
      throw CommandFailure{CommandExit::Invalid, "missing_argument",
                           "\"" + command.name + "\" requires --" + param.name};
    }
  }
  return args;
}

void CheckDocumentIdentity(const CommandTarget& target, const LevelJson& args) {
  const DocumentState document = target.document();
  if (args.contains("document") && args["document"] != document.id) {
    throw CommandFailure{
        CommandExit::Conflict, "document_mismatch",
        "The editor's open document is " +
            (document.open ? "\"" + document.id + "\"" : "none") + ", not " +
            args["document"].dump() + ". Inspect the session again"};
  }
}
}  // namespace

void CommandRegistry::add(Command command) {
  for (const Command& existing : registered) {
    if (existing.name == command.name &&
        ((existing.supportsFile && command.supportsFile) ||
         (existing.supportsEditor && command.supportsEditor))) {
      throw std::logic_error("Duplicate command target: " + command.name);
    }
  }
  registered.push_back(std::move(command));
}

const Command* CommandRegistry::find(const std::string& name,
                                     const std::string& target) const {
  const Command* fallback = nullptr;
  for (const Command& command : registered) {
    if (command.name != name) {
      continue;
    }
    fallback = &command;
    if (target.empty() || (target == "file" && command.supportsFile) ||
        (target == "editor" && command.supportsEditor)) {
      return &command;
    }
  }
  return fallback;
}

LevelJson CommandRegistry::dispatch(CommandTarget& target,
                                    const CommandRequest& request) const {
  LevelJson result;
  const Command* command = nullptr;
  try {
    // "objects list" and "commands describe apply" both start with a shorter
    // command's words, so the longest registered name wins
    size_t nameWords = 0;
    for (size_t count = request.words.size(); count > 0; count--) {
      command = find(JoinWords(request.words, 0, count), target.kind());
      if (command != nullptr) {
        nameWords = count;
        break;
      }
    }
    if (command == nullptr) {
      throw CommandFailure{
          CommandExit::Invalid, "unknown_command",
          "Unknown command \"" +
              JoinWords(request.words, 0, request.words.size()) +
              "\". Run `commands` to list them"};
    }

    const bool live = std::string(target.kind()) == "editor";
    if (live ? !command->supportsEditor : !command->supportsFile) {
      throw CommandFailure{
          CommandExit::Invalid, "unsupported_target",
          "\"" + command->name + "\" needs " +
              (live ? "--file <level>" : "--editor <session>") +
              "; it is not available for this target"};
    }

    const LevelJson args = PrepareArguments(
        *command, request.args,
        JoinWords(request.words, nameWords, request.words.size() - nameWords),
        live);
    CheckDocumentIdentity(target, args);
    if (command->mutates) {
      RequireRevision(target, args);
    }

    result = {{"ok", true},
              {"code", "ok"},
              {"changed", false},
              {"affected_ids", LevelJson::array()},
              {"warnings", LevelJson::array()},
              {"errors", LevelJson::array()}};
    result.update(command->run(target, args));
    result["exit_code"] = result["ok"] == true ? 0 : (int)CommandExit::Invalid;
  } catch (const CommandFailure& failure) {
    result = FailureResult(failure);
  }

  result["command"] = command != nullptr ? LevelJson(command->name) : nullptr;
  result["target"] = {{"kind", target.kind()}};
  if (!target.session().empty()) {
    result["target"]["session"] = target.session();
  }
  result["document"] = DocumentJson(target);
  if (!request.requestId.empty()) {
    result["request_id"] = request.requestId;
  }
  return result;
}

LevelJson FailureResult(const CommandFailure& failure) {
  LevelJson errors = LevelJson::array();
  for (const LevelDiagnostic& diagnostic : failure.errors) {
    errors.push_back(DiagnosticToJson(diagnostic));
  }
  if (errors.empty()) {
    errors.push_back({{"severity", "error"},
                      {"code", failure.code},
                      {"message", failure.message}});
  }
  return {{"ok", false},
          {"code", failure.code},
          {"category", ExitCodeName(failure.exit)},
          {"message", failure.message},
          {"changed", false},
          {"affected_ids", LevelJson::array()},
          {"warnings", LevelJson::array()},
          {"errors", errors},
          {"exit_code", (int)failure.exit}};
}

int CommandExitCode(const LevelJson& result) {
  return result.value("exit_code", (int)CommandExit::Invalid);
}

const Level& RequireLevel(const CommandTarget& target) {
  const DocumentState document = target.document();
  if (document.level != nullptr) {
    return *document.level;
  }
  const LevelDiagnostics errors = target.loadErrors();
  if (errors.empty()) {
    throw CommandFailure{CommandExit::Invalid, "no_document",
                         "No level is open"};
  }
  throw CommandFailure{
      CommandExit::Invalid, "invalid_document",
      "The level cannot be read: " + DescribeDiagnostic(errors.front()),
      errors};
}

void RequireRevision(const CommandTarget& target, const LevelJson& args) {
  const DocumentState document = target.document();
  if (!args.contains("expect")) {
    throw CommandFailure{CommandExit::Invalid, "missing_argument",
                         "--expect <revision> is required; use the revision "
                         "from `inspect`"};
  }
  if (!document.open) {
    throw CommandFailure{CommandExit::Invalid, "no_document",
                         "No level is open"};
  }
  if (args["expect"] != document.revision) {
    throw CommandFailure{CommandExit::Conflict, "revision_conflict",
                         "Expected revision " + args["expect"].dump() +
                             " but the document is at \"" + document.revision +
                             "\". Inspect it again and reapply"};
  }
}

std::vector<CommandParam> PageParams() {
  return {
      {"limit", "integer", false,
       "Records per page, 1 to 200. Defaults to 50."},
      {"cursor", "string", false,
       "next_cursor from the previous page of the same query"},
  };
}

CommandPage ReadPage(const LevelJson& args, const std::string& scope) {
  CommandPage page;
  if (args.contains("limit")) {
    const long long limit = args["limit"].get<long long>();
    if (limit < 1 || limit > (long long)maximumPageSize) {
      throw CommandFailure{
          CommandExit::Invalid, "invalid_argument",
          "--limit must be between 1 and " + std::to_string(maximumPageSize)};
    }
    page.limit = (size_t)limit;
  }
  if (args.contains("cursor")) {
    const std::string cursor = args["cursor"].get<std::string>();
    const size_t separator = cursor.find(':');
    const std::string offset =
        separator == std::string::npos ? "" : cursor.substr(separator + 1);
    const bool numeric =
        !offset.empty() && offset.size() < 10 &&
        std::all_of(
            offset.begin(), offset.end(),
            [](unsigned char c) { return std::isdigit(c) != 0; });
    if (!numeric || cursor.substr(0, separator) != HashText(scope)) {
      throw CommandFailure{
          CommandExit::Conflict, "stale_cursor",
          "The cursor belongs to a different query or an earlier document "
          "revision. Run the command again without --cursor"};
    }
    page.offset = (size_t)std::stoul(offset);
  }
  return page;
}

void WritePage(LevelJson& result, const CommandPage& page, size_t total,
               const std::string& scope) {
  const size_t next = std::min(page.offset + page.limit, total);
  result["total"] = total;
  result["has_more"] = next < total;
  result["next_cursor"] =
      next < total ? LevelJson(HashText(scope) + ":" + std::to_string(next))
                   : LevelJson();
}

std::string HashText(const std::string& text) {
  // FNV-1a: change detection, not security
  uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char byte : text) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  char hex[17];
  std::snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)hash);
  return hex;
}
