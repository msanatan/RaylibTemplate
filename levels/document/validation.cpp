#include "levels/document/validation.h"

#include <cctype>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>

#include "levels/document/level_index.h"
#include "levels/document/serialization.h"

namespace {
void Add(LevelDiagnostics& diagnostics, const LevelObject& object,
         const std::string& code, const std::string& path,
         const std::string& message) {
  LevelDiagnostic diagnostic;
  diagnostic.code = code;
  diagnostic.objectId = object.id;
  diagnostic.path = path;
  diagnostic.message = message;
  diagnostics.push_back(diagnostic);
}

bool IsFinite(const LevelVec3& value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

bool IsPositive(double value) { return std::isfinite(value) && value > 0.0; }

bool IsAssetPath(const std::string& path) {
  if (path.empty() || path.front() == '/' ||
      path.find('\\') != std::string::npos ||
      path.find(':') != std::string::npos) {
    return false;
  }
  for (const std::filesystem::path& part : std::filesystem::path(path)) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

void ValidateShape(LevelDiagnostics& diagnostics, const LevelObject& object) {
  const LevelShape& shape = object.shape;
  bool valid = true;
  if (object.kind == LevelKind::Box) {
    valid = IsPositive(shape.size.x) && IsPositive(shape.size.y) &&
            IsPositive(shape.size.z);
  } else if (object.kind == LevelKind::Plane) {
    valid = IsPositive(shape.size.x) && IsPositive(shape.size.z);
  } else if (object.kind == LevelKind::Sphere) {
    valid = IsPositive(shape.radius);
  } else if (object.kind == LevelKind::Cylinder) {
    valid = IsPositive(shape.radius) && IsPositive(shape.height);
  }
  if (!valid) {
    Add(diagnostics, object, "invalid_shape", "/shape",
        "Dimensions must be positive, finite sizes in metres");
  }
}

void ValidateAppearance(LevelDiagnostics& diagnostics,
                        const LevelObject& object) {
  const LevelAppearance& appearance = object.appearance;
  if (!IsValidColor(appearance.color)) {
    Add(diagnostics, object, "invalid_color", "/appearance/color",
        "Color must be a hex string such as \"#E62937\"");
  }
  if (appearance.texture.has_value() && !IsAssetPath(*appearance.texture)) {
    Add(diagnostics, object, "invalid_asset_path", "/appearance/texture",
        "Texture must be a path relative to the assets directory, such as "
        "\"textures/crate.png\"");
  }
  if (!std::isfinite(appearance.tiling.x) ||
      !std::isfinite(appearance.tiling.y) ||
      !std::isfinite(appearance.offset.x) ||
      !std::isfinite(appearance.offset.y)) {
    Add(diagnostics, object, "invalid_appearance", "/appearance",
        "Tiling and offset must be finite numbers");
  }
}

void ValidateCamera(LevelDiagnostics& diagnostics, const LevelObject& object) {
  const LevelCameraSettings& camera = object.camera;
  if (!camera.orthographic &&
      !(std::isfinite(camera.fov) && camera.fov > 0.0 && camera.fov < 180.0)) {
    Add(diagnostics, object, "invalid_camera", "/camera/fov",
        "Vertical field of view must be between 0 and 180 degrees, exclusive");
  }
  if (camera.orthographic && !IsPositive(camera.size)) {
    Add(diagnostics, object, "invalid_camera", "/camera/size",
        "Orthographic size must be a positive height in metres");
  }
  if (!(IsPositive(camera.nearPlane) && std::isfinite(camera.farPlane) &&
        camera.nearPlane < camera.farPlane)) {
    Add(diagnostics, object, "invalid_camera", "/camera/near",
        "Clipping distances must satisfy 0 < near < far");
  }
}
}  // namespace

bool IsValidColor(const std::string& color) {
  if (color.size() != 7 || color[0] != '#') {
    return false;
  }
  for (size_t i = 1; i < color.size(); i++) {
    if (std::isxdigit((unsigned char)color[i]) == 0) {
      return false;
    }
  }
  return true;
}

LevelDiagnostics ValidateLevel(const Level& level) {
  LevelDiagnostics diagnostics;
  const LevelIndex index(level);

  std::set<std::string> ids;
  for (const LevelObject& object : level.objects) {
    if (!IsLevelId(object.id)) {
      Add(diagnostics, object, "invalid_id", "/id",
          "An ID must be a UUID such as "
          "\"3f2b8c1e-5d47-4a90-b6e2-9c1d0a7f4e55\". Omit \"id\" from a "
          "create operation to have one generated");
    }
    if (!ids.insert(object.id).second) {
      Add(diagnostics, object, "duplicate_id", "/id",
          "More than one object has the ID \"" + object.id + "\"");
    }
  }

  std::map<std::optional<std::string>, std::vector<const LevelObject*>>
      siblings;
  for (const LevelObject& object : level.objects) {
    if (object.parent.has_value() && ids.count(*object.parent) == 0) {
      Add(diagnostics, object, "unknown_parent", "/parent",
          "Parent \"" + *object.parent + "\" does not exist");
    } else if (object.parent == object.id ||
               index.isDescendant(object.id, object.id)) {
      Add(diagnostics, object, "hierarchy_cycle", "/parent",
          "The object is its own ancestor");
    }
    siblings[object.parent].push_back(&object);

    const LevelTransform& transform = object.transform;
    if (!IsFinite(transform.position) || !IsFinite(transform.rotation)) {
      Add(diagnostics, object, "invalid_transform", "/transform",
          "Position and rotation must be finite numbers");
    }
    if (!IsPositive(transform.scale.x) || !IsPositive(transform.scale.y) ||
        !IsPositive(transform.scale.z)) {
      Add(diagnostics, object, "invalid_scale", "/transform/scale",
          "Scale must be positive and finite on every axis");
    }

    if (IsPrimitive(object.kind)) {
      ValidateShape(diagnostics, object);
      ValidateAppearance(diagnostics, object);
    }
    if (object.kind == LevelKind::Camera) {
      ValidateCamera(diagnostics, object);
    }

    LevelJson properties = object.properties;
    ForEachReference(properties, [&](std::string& id, const std::string& path) {
      if (ids.count(id) == 0) {
        Add(diagnostics, object, "unknown_reference", "/properties" + path,
            "Referenced object \"" + id + "\" does not exist");
      }
    });
  }

  for (const auto& [parent, children] : siblings) {
    std::set<int> orders;
    for (const LevelObject* child : children) {
      orders.insert(child->order);
    }
    const bool contiguous = orders.size() == children.size() &&
                            *orders.begin() == 0 &&
                            *orders.rbegin() == (int)children.size() - 1;
    if (!contiguous) {
      // Reported once per group, on the first sibling, so one mistake does
      // not flood the output
      Add(diagnostics, *children.front(), "invalid_sibling_order", "/order",
          "Sibling order under " +
              (parent.has_value() ? "\"" + *parent + "\"" : "the root") +
              " must be the integers 0 to " +
              std::to_string(children.size() - 1) + ", each used once");
    }
  }

  if (level.camera.has_value()) {
    const LevelObject* camera = level.find(*level.camera);
    if (camera == nullptr || camera->kind != LevelKind::Camera) {
      LevelDiagnostic diagnostic;
      diagnostic.code = "invalid_level_camera";
      diagnostic.path = "/camera";
      diagnostic.message =
          camera == nullptr
              ? "The level camera \"" + *level.camera + "\" does not exist"
              : "The level camera \"" + *level.camera +
                    "\" is not a camera object";
      diagnostics.push_back(diagnostic);
    }
  }
  return diagnostics;
}

std::optional<Level> ParseValidLevel(const std::string& text,
                                     LevelDiagnostics& errors) {
  LevelParseResult parsed = ParseLevel(text);
  errors = parsed.errors;
  if (!parsed.level.has_value()) {
    return std::nullopt;
  }
  errors = ValidateLevel(*parsed.level);
  return errors.empty() ? parsed.level : std::nullopt;
}

LevelDiagnostics ValidateLevelAssets(const Level& level,
                                     const std::string& assetsDirectory) {
  LevelDiagnostics diagnostics;
  for (const LevelObject& object : level.objects) {
    if (!IsPrimitive(object.kind) || !object.appearance.texture.has_value()) {
      continue;
    }
    std::error_code error;
    const std::filesystem::path file =
        std::filesystem::path(assetsDirectory) / *object.appearance.texture;
    if (!std::filesystem::is_regular_file(file, error)) {
      Add(diagnostics, object, "missing_asset", "/appearance/texture",
          "Texture \"" + *object.appearance.texture +
              "\" was not found in the assets directory");
      diagnostics.back().warning = true;
    }
  }
  return diagnostics;
}

LevelJson DiagnosticToJson(const LevelDiagnostic& diagnostic) {
  LevelJson json;
  json["severity"] = diagnostic.warning ? "warning" : "error";
  json["code"] = diagnostic.code;
  json["message"] = diagnostic.message;
  if (!diagnostic.objectId.empty()) {
    json["object_id"] = diagnostic.objectId;
  }
  if (!diagnostic.path.empty()) {
    json["path"] = diagnostic.path;
  }
  if (diagnostic.line > 0) {
    json["line"] = diagnostic.line;
    json["column"] = diagnostic.column;
  }
  if (diagnostic.operation >= 0) {
    json["operation"] = diagnostic.operation;
  }
  return json;
}

std::string DescribeDiagnostic(const LevelDiagnostic& diagnostic) {
  std::string text = diagnostic.message;
  if (!diagnostic.objectId.empty()) {
    text += " (object " + diagnostic.objectId + ")";
  }
  if (!diagnostic.path.empty()) {
    text += " at " + diagnostic.path;
  }
  if (diagnostic.line > 0) {
    text += " (line " + std::to_string(diagnostic.line) + ", column " +
            std::to_string(diagnostic.column) + ")";
  }
  return text;
}
