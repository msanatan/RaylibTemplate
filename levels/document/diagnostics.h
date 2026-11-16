#ifndef GBM_LEVEL_DIAGNOSTICS_H
#define GBM_LEVEL_DIAGNOSTICS_H

#include <string>
#include <vector>

#include "levels/document/level.h"

struct LevelDiagnostic {
  bool warning = false;
  std::string code;
  std::string message;
  std::string objectId;
  std::string path;
  int line = 0;
  int column = 0;
  int operation = -1;
};

using LevelDiagnostics = std::vector<LevelDiagnostic>;

LevelJson DiagnosticToJson(const LevelDiagnostic& diagnostic);
std::string DescribeDiagnostic(const LevelDiagnostic& diagnostic);

#endif  // !GBM_LEVEL_DIAGNOSTICS_H
