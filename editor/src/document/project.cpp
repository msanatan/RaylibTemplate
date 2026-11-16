#include "document/project.h"

#include <filesystem>
#include <sstream>

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

namespace {
namespace fs = std::filesystem;

std::vector<std::string> Strings(const toml::node_view<toml::node>& node) {
  std::vector<std::string> values;
  if (const toml::array* array = node.as_array()) {
    for (const toml::node& item : *array) {
      values.push_back(item.value_or(std::string()));
    }
  }
  return values;
}
}  // namespace

std::string Project::resolve(const std::string& path) const {
  const fs::path given(path);
  return (given.is_absolute() ? given : fs::path(root) / given)
      .lexically_normal()
      .string();
}

bool LoadProject(const std::string& root,
                 const std::string& fallbackGameExecutable, Project& project,
                 std::string& error) {
  std::error_code ignored;
  const fs::path directory = fs::absolute(root, ignored).lexically_normal();
  const fs::path settings = directory / "project.toml";
  toml::parse_result parsed = toml::parse_file(settings.string());
  if (!parsed) {
    std::ostringstream message;
    message << parsed.error();
    error = "Could not read " + settings.string() + ": " + message.str();
    return false;
  }
  toml::table& table = parsed.table();

  project.root = directory.string();
  project.assetsDirectory = (directory / "assets").string();
  project.preferencesDirectory = (directory / ".scenic").string();
  const std::string level = table["editor"]["level"].value_or(std::string());
  project.defaultLevel = level.empty() ? "" : project.resolve(level);
  const std::string executable =
      table["game"]["executable"].value_or(std::string());
  project.gameExecutable =
      executable.empty() ? fallbackGameExecutable : project.resolve(executable);
  project.playArguments = Strings(table["game"]["play_args"]);
  project.resetArguments = Strings(table["game"]["reset_args"]);
  return true;
}
