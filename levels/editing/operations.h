#ifndef GBM_LEVEL_OPERATIONS_H
#define GBM_LEVEL_OPERATIONS_H

#include <string>
#include <vector>

#include "levels/document/diagnostics.h"
#include "levels/document/level.h"

struct LevelEditResult {
  bool ok = false;
  bool changed = false;
  Level level;
  LevelDiagnostics errors;
  // IDs the batch generated: {"operation", "id"} for a create without one,
  // {"operation", "ids": {old: new}} for a duplicate
  LevelJson generatedIds = LevelJson::array();
};

LevelEditResult ApplyLevelOperations(const Level& level,
                                     const LevelJson& request);

LevelJson DescribeLevelChanges(const Level& before, const Level& after,
                               std::vector<std::string>& affectedIds);

const LevelJson& LevelOperationsSchema();

#endif  // !GBM_LEVEL_OPERATIONS_H
