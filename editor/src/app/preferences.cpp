#include "app/preferences.h"

#include <filesystem>
#include <fstream>

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

namespace {
std::string File(const std::string& directory) {
  return (std::filesystem::path(directory) / "preferences.toml").string();
}
}  // namespace

Preferences LoadPreferences(const std::string& directory) {
  Preferences preferences;
  const toml::parse_result parsed = toml::parse_file(File(directory));
  if (parsed) {
    preferences.darkTheme =
        parsed.table()["theme"].value_or(std::string("dark")) != "light";
  }
  return preferences;
}

void SavePreferences(const std::string& directory,
                     const Preferences& preferences) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  std::ofstream(File(directory))
      << "theme = \"" << (preferences.darkTheme ? "dark" : "light") << "\"\n";
}
