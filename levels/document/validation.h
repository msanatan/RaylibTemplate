#ifndef GBM_LEVEL_VALIDATION_H
#define GBM_LEVEL_VALIDATION_H

#include <optional>
#include <string>

#include "levels/document/diagnostics.h"
#include "levels/document/level.h"

LevelDiagnostics ValidateLevel(const Level& level);

LevelDiagnostics ValidateLevelAssets(const Level& level,
                                     const std::string& assetsDirectory);

// The level when `text` parses and validates; otherwise empty, with `errors`
// saying why
std::optional<Level> ParseValidLevel(const std::string& text,
                                     LevelDiagnostics& errors);

bool IsValidColor(const std::string& color);

#endif  // !GBM_LEVEL_VALIDATION_H
