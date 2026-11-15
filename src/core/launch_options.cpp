#include "core/launch_options.h"

#include <map>

bool ParseLaunchOptions(const std::vector<std::string>& arguments,
                        LaunchOptions& options, std::string& error) {
  const std::map<std::string, std::string*> values = {
      {"--scene", &options.scene},
      {"--script", &options.script},
      {"--session", &options.session},
      {"--log", &options.log},
  };

  for (size_t i = 0; i < arguments.size(); i += 2) {
    const auto value = values.find(arguments[i]);
    if (value == values.end()) {
      error = "Unknown option " + arguments[i];
      return false;
    }
    if (i + 1 >= arguments.size()) {
      error = "Missing value for " + arguments[i];
      return false;
    }
    *value->second = arguments[i + 1];
  }
  return true;
}
