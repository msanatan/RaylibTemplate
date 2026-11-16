#ifndef SCENIC_PREFERENCES_H
#define SCENIC_PREFERENCES_H

#include <string>

struct Preferences {
  bool darkTheme = true;
};

Preferences LoadPreferences(const std::string& directory);
void SavePreferences(const std::string& directory,
                     const Preferences& preferences);

#endif  // !SCENIC_PREFERENCES_H
