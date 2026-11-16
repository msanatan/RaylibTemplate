#include "levels/runtime/loading.h"

#include <map>

#include "levels/document/validation.h"
#include "raylib.h"

namespace {
std::map<std::string, Level>& LastValidLevels() {
  static std::map<std::string, Level> levels;
  return levels;
}
}  // namespace

LoadedLevel LoadGameLevel(const std::string& path) {
  LevelDiagnostics errors;
  std::optional<Level> level;

  // raylib's loader also reads from the release asset pack
  char* text = LoadFileText(path.c_str());
  if (text == nullptr) {
    LevelDiagnostic missing;
    missing.message = "The file could not be read";
    errors.push_back(missing);
  } else {
    level = ParseValidLevel(text, errors);
    UnloadFileText(text);
  }

  if (level.has_value()) {
    LastValidLevels()[path] = *level;
    TraceLog(LOG_INFO, "LEVEL: Loaded [%s] with %d objects", path.c_str(),
             (int)level->objects.size());
    return {*level, true};
  }

  for (const LevelDiagnostic& error : errors) {
    TraceLog(LOG_ERROR, "LEVEL: [%s] %s", path.c_str(),
             DescribeDiagnostic(error).c_str());
  }
  const auto lastValid = LastValidLevels().find(path);
  if (lastValid == LastValidLevels().end()) {
    TraceLog(LOG_ERROR, "LEVEL: [%s] is invalid; using an empty level",
             path.c_str());
    return {Level(), false};
  }
  TraceLog(LOG_WARNING, "LEVEL: [%s] is invalid; keeping the last valid level",
           path.c_str());
  return {lastValid->second, false};
}
