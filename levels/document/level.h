#ifndef GBM_LEVEL_H
#define GBM_LEVEL_H

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

// Authored 3D worlds, shared by the game and the Scenic editor. A level is
// unrelated to the application's Scene screens.
//
// Conventions: one unit is one metre, +Y is up, the ground is the XZ plane,
// and objects and cameras face along their local -Z axis.

using LevelJson = nlohmann::json;

inline constexpr int levelFileVersion = 1;

struct LevelVec3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  bool operator==(const LevelVec3&) const = default;
};

struct LevelVec2 {
  double x = 0.0;
  double y = 0.0;
  bool operator==(const LevelVec2&) const = default;
};

// Relative to the parent. Rotation is Euler degrees applied as roll (Z), then
// pitch (X), then yaw (Y); see transform.h for the matrix composition.
struct LevelTransform {
  LevelVec3 position;
  LevelVec3 rotation;
  LevelVec3 scale = {1.0, 1.0, 1.0};
  bool operator==(const LevelTransform&) const = default;
};

enum class LevelKind { Group, Marker, Box, Sphere, Cylinder, Plane, Camera };

struct LevelShape {
  LevelVec3 size = {1.0, 1.0, 1.0};
  double radius = 0.5;
  double height = 1.0;
  bool operator==(const LevelShape&) const = default;
};

struct LevelAppearance {
  std::string color = "#FFFFFF";
  std::optional<std::string> texture;
  LevelVec2 tiling = {1.0, 1.0};
  LevelVec2 offset = {0.0, 0.0};
  bool operator==(const LevelAppearance&) const = default;
};

struct LevelCameraSettings {
  bool orthographic = false;
  double fov = 45.0;
  double size = 10.0;
  double nearPlane = 0.1;
  double farPlane = 1000.0;
  bool operator==(const LevelCameraSettings&) const = default;
};

struct LevelObject {
  std::string id;
  std::string name;
  LevelKind kind = LevelKind::Group;
  std::optional<std::string> parent;
  int order = 0;
  LevelTransform transform;
  LevelShape shape;
  LevelAppearance appearance;
  LevelCameraSettings camera;
  // Game-specific data the editor preserves without interpreting. An object
  // of the form {"$ref": "<object id>"} is a reference to another object.
  LevelJson properties = LevelJson::object();
  bool operator==(const LevelObject&) const = default;
};

struct Level {
  int version = levelFileVersion;
  std::optional<std::string> camera;
  std::vector<LevelObject> objects;
  bool operator==(const Level&) const = default;

  const LevelObject* find(const std::string& id) const;
  LevelObject* find(const std::string& id);
};

// Object IDs are UUIDs, so objects copied between levels or merged from
// branches cannot collide the way readable names would
std::string NewLevelId();
bool IsLevelId(const std::string& id);

const char* LevelKindName(LevelKind kind);
std::optional<LevelKind> LevelKindFromName(const std::string& name);
bool IsPrimitive(LevelKind kind);

LevelVec3 ShapeHalfExtents(const LevelObject& object);

// Visits every {"$ref": id} object inside custom properties
void ForEachReference(
    LevelJson& properties,
    const std::function<void(std::string& id, const std::string& path)>& visit,
    const std::string& path = "");

#endif  // !GBM_LEVEL_H
