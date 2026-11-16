#include "levels/document/level.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <random>
#include <utility>

namespace {
const std::array<std::pair<LevelKind, const char*>, 7> kindNames = {{
    {LevelKind::Group, "group"},
    {LevelKind::Marker, "marker"},
    {LevelKind::Box, "box"},
    {LevelKind::Sphere, "sphere"},
    {LevelKind::Cylinder, "cylinder"},
    {LevelKind::Plane, "plane"},
    {LevelKind::Camera, "camera"},
}};

}  // namespace

const LevelObject* Level::find(const std::string& id) const {
  for (const LevelObject& object : objects) {
    if (object.id == id) {
      return &object;
    }
  }
  return nullptr;
}

LevelObject* Level::find(const std::string& id) {
  return const_cast<LevelObject*>(std::as_const(*this).find(id));
}

std::string NewLevelId() {
  static std::mt19937_64 generator{std::random_device{}()};
  static const char* const digits = "0123456789abcdef";
  std::string id = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
  for (char& character : id) {
    const unsigned int random = (unsigned int)(generator() & 0xFU);
    if (character == 'x') {
      character = digits[random];
    } else if (character == 'y') {
      character = digits[(random & 0x3U) | 0x8U];
    }
  }
  return id;
}

bool IsLevelId(const std::string& id) {
  if (id.size() != 36) {
    return false;
  }
  for (size_t i = 0; i < id.size(); i++) {
    const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
    if (dash ? id[i] != '-' : std::isxdigit((unsigned char)id[i]) == 0) {
      return false;
    }
  }
  return true;
}

const char* LevelKindName(LevelKind kind) {
  for (const auto& [value, name] : kindNames) {
    if (value == kind) {
      return name;
    }
  }
  return "group";
}

std::optional<LevelKind> LevelKindFromName(const std::string& name) {
  for (const auto& [value, kindName] : kindNames) {
    if (name == kindName) {
      return value;
    }
  }
  return std::nullopt;
}

bool IsPrimitive(LevelKind kind) {
  return kind == LevelKind::Box || kind == LevelKind::Sphere ||
         kind == LevelKind::Cylinder || kind == LevelKind::Plane;
}

LevelVec3 ShapeHalfExtents(const LevelObject& object) {
  const LevelShape& shape = object.shape;
  switch (object.kind) {
    case LevelKind::Box:
      return {shape.size.x / 2.0, shape.size.y / 2.0, shape.size.z / 2.0};
    case LevelKind::Plane:
      return {shape.size.x / 2.0, 0.0, shape.size.z / 2.0};
    case LevelKind::Sphere:
      return {shape.radius, shape.radius, shape.radius};
    case LevelKind::Cylinder:
      return {shape.radius, shape.height / 2.0, shape.radius};
    default:
      return {0.0, 0.0, 0.0};
  }
}

void ForEachReference(
    LevelJson& properties,
    const std::function<void(std::string& id, const std::string& path)>& visit,
    const std::string& path) {
  if (properties.is_object()) {
    if (properties.size() == 1 && properties.contains("$ref") &&
        properties["$ref"].is_string()) {
      visit(properties["$ref"].get_ref<std::string&>(), path);
      return;
    }
    for (auto& [key, value] : properties.items()) {
      std::string child = path;
      child.append("/").append(key);
      ForEachReference(value, visit, child);
    }
  } else if (properties.is_array()) {
    for (size_t i = 0; i < properties.size(); i++) {
      std::string child = path;
      child.append("/").append(std::to_string(i));
      ForEachReference(properties[i], visit, child);
    }
  }
}
