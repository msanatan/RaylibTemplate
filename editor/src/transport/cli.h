#ifndef SCENIC_CLI_H
#define SCENIC_CLI_H

#include <iosfwd>
#include <string>
#include <vector>

#include "levels/editing/commands.h"

struct CliArguments {
  std::string editor;
  std::string file;
  std::string assets;
  std::string requestId;
  bool hasEditor = false;
  bool hasFile = false;
  std::vector<std::string> words;
  LevelJson args = LevelJson::object();
};

CliArguments ParseCliArguments(const std::vector<std::string>& arguments,
                               const CommandRegistry& registry);

int RunCli(const std::vector<std::string>& arguments, std::istream& in,
           std::ostream& out);

#endif  // !SCENIC_CLI_H
