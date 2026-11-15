#include "core/testing/script.h"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

#include "raylib.h"

namespace {
int KeyFromName(const std::string& name) {
  static const std::map<std::string, int> namedKeys = {
      {"SPACE", KEY_SPACE},
      {"ENTER", KEY_ENTER},
      {"ESCAPE", KEY_ESCAPE},
      {"TAB", KEY_TAB},
      {"BACKSPACE", KEY_BACKSPACE},
      {"UP", KEY_UP},
      {"DOWN", KEY_DOWN},
      {"LEFT", KEY_LEFT},
      {"RIGHT", KEY_RIGHT},
      {"SHIFT", KEY_LEFT_SHIFT},
      {"CONTROL", KEY_LEFT_CONTROL},
  };

  if (name.size() == 1 && std::isalnum((unsigned char)name[0])) {
    return std::toupper((unsigned char)name[0]);
  }
  const auto key = namedKeys.find(name);
  return key == namedKeys.end() ? KEY_NULL : key->second;
}
}  // namespace

bool LoadPlaytestScript(const std::string& scriptFile,
                        std::vector<PlaytestCommand>& commands) {
  std::ifstream file(scriptFile);
  if (!file) {
    TraceLog(LOG_ERROR, "PLAYTEST: Failed to open [%s]", scriptFile.c_str());
    return false;
  }

  std::string line;
  for (int lineNumber = 1; std::getline(file, line); lineNumber++) {
    std::istringstream words(line.substr(0, line.find('#')));
    PlaytestCommand command;
    command.line = lineNumber;
    if (!(words >> command.name)) {
      continue;
    }

    std::string keyName;
    bool valid = true;
    if (command.name == "press") {
      valid = (bool)(words >> keyName);
    } else if (command.name == "hold") {
      valid = (bool)(words >> keyName >> command.frames);
    } else if (command.name == "click") {
      valid = (bool)(words >> command.x >> command.y);
    } else if (command.name == "touch") {
      valid = (bool)(words >> command.x >> command.y >> command.frames);
    } else if (command.name == "wait") {
      valid = (bool)(words >> command.frames);
    } else if (command.name == "shot" || command.name == "expect") {
      std::getline(words >> std::ws, command.text);
      command.text.erase(command.text.find_last_not_of(" \t\r") + 1);
      valid = !command.text.empty();
    } else if (command.name != "dump" && command.name != "quit") {
      valid = false;
    }

    command.key = KeyFromName(keyName);
    if (!valid || command.frames < 1 ||
        (!keyName.empty() && command.key == KEY_NULL)) {
      TraceLog(LOG_ERROR, "PLAYTEST: [%s:%d] Invalid command: %s",
               scriptFile.c_str(), lineNumber, line.c_str());
      return false;
    }
    commands.push_back(command);
  }
  return true;
}
