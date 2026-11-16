#ifndef GBM_LEVEL_COMMANDS_H
#define GBM_LEVEL_COMMANDS_H

#include <exception>
#include <functional>
#include <string>
#include <vector>

#include "levels/document/diagnostics.h"
#include "levels/document/level.h"

// The command interface agents and the editor share. Commands are plain data
// in and out, so the scenic-cli, the editor's local socket, and tests all
// reach the same handlers.

enum class CommandExit {
  Success = 0,
  Invalid = 2,
  Conflict = 3,
  Io = 4,
  Noncanonical = 5,
};

struct CommandFailure : std::exception {
  CommandFailure(CommandExit exit, std::string code, std::string message,
                 LevelDiagnostics errors = {})
      : exit(exit),
        code(std::move(code)),
        message(std::move(message)),
        errors(std::move(errors)) {}

  const char* what() const noexcept override { return message.c_str(); }

  CommandExit exit;
  std::string code;
  std::string message;
  LevelDiagnostics errors;
};

struct DocumentState {
  bool open = false;
  std::string id;
  std::string path;
  std::string revision;
  bool dirty = false;
  const Level* level = nullptr;
};

class CommandTarget {
 public:
  virtual ~CommandTarget() = default;

  virtual const char* kind() const = 0;
  virtual std::string session() const { return ""; }
  virtual DocumentState document() const = 0;
  virtual std::string assetsDirectory() const = 0;
  virtual LevelDiagnostics loadErrors() const { return {}; }

  virtual std::string commit(const Level& level, const std::string& label) = 0;
};

struct CommandParam {
  std::string name;
  std::string type;
  bool required = false;
  std::string description;
};

struct Command {
  std::string name;
  std::string description;
  bool supportsFile = false;
  bool supportsEditor = false;
  bool mutates = false;
  bool hasSideEffects = false;
  std::vector<CommandParam> params;
  std::string resultDescription;
  std::string positional;
  std::function<LevelJson(CommandTarget&, const LevelJson& args)> run;
};

struct CommandRequest {
  std::vector<std::string> words;
  LevelJson args = LevelJson::object();
  std::string requestId;
};

class CommandRegistry {
 public:
  void add(Command command);
  const std::vector<Command>& commands() const { return registered; }
  const Command* find(const std::string& name,
                      const std::string& target = "") const;

  LevelJson dispatch(CommandTarget& target,
                     const CommandRequest& request) const;

 private:
  std::vector<Command> registered;
};

void RegisterDocumentCommands(CommandRegistry& registry);

LevelJson FailureResult(const CommandFailure& failure);
int CommandExitCode(const LevelJson& result);

const Level& RequireLevel(const CommandTarget& target);
void RequireRevision(const CommandTarget& target, const LevelJson& args);

inline constexpr size_t defaultPageSize = 50;
inline constexpr size_t maximumPageSize = 200;
inline constexpr size_t maximumValueLength = 4000;

struct CommandPage {
  size_t offset = 0;
  size_t limit = defaultPageSize;
};

CommandPage ReadPage(const LevelJson& args, const std::string& scope);
void WritePage(LevelJson& result, const CommandPage& page, size_t total,
               const std::string& scope);
std::vector<CommandParam> PageParams();

std::string HashText(const std::string& text);

#endif  // !GBM_LEVEL_COMMANDS_H
