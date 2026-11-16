#ifndef GBM_LEVEL_LOADING_H
#define GBM_LEVEL_LOADING_H

#include <string>

#include "levels/document/level.h"

// Reads and validates a level for the game. A file that fails to load keeps
// the last valid level read from the same path, so a bad save cannot take
// down a running game; errors are logged with the LEVEL: prefix. Before any
// valid load the result is an empty level.
struct LoadedLevel {
  Level level;
  bool current = false;
};

LoadedLevel LoadGameLevel(const std::string& path);

#endif  // !GBM_LEVEL_LOADING_H
