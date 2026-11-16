#ifndef GBM_LEVEL_SERIALIZATION_H
#define GBM_LEVEL_SERIALIZATION_H

#include <optional>
#include <string>

#include "levels/document/diagnostics.h"
#include "levels/document/level.h"

struct LevelParseResult {
  std::optional<Level> level;
  LevelDiagnostics errors;
};

LevelParseResult ParseLevel(const std::string& text);

std::string SerializeLevel(const Level& level);

LevelJson ObjectToJson(const LevelObject& object);

struct LevelFieldErrors {
  LevelDiagnostics& errors;
  std::string objectId;
  std::string basePath;
  int operation = -1;

  void add(const std::string& code, const std::string& path,
           const std::string& message) const;
};

// Each reader updates only the fields present in `source`, so edits can supply
// a subset. Unknown fields and wrong types are reported and return false.
bool ReadTransformFields(const LevelJson& source, LevelTransform& transform,
                         const LevelFieldErrors& errors);
bool ReadShapeFields(const LevelJson& source, LevelKind kind, LevelShape& shape,
                     const LevelFieldErrors& errors);
bool ReadAppearanceFields(const LevelJson& source, LevelAppearance& appearance,
                          const LevelFieldErrors& errors);
bool ReadCameraFields(const LevelJson& source, LevelCameraSettings& camera,
                      const LevelFieldErrors& errors);

// Reads a whole object in file form. `orderRequired` is false for the create
// operation, which takes an insertion index instead.
bool ReadObject(const LevelJson& source, LevelObject& object,
                const LevelFieldErrors& errors, bool orderRequired);

#endif  // !GBM_LEVEL_SERIALIZATION_H
