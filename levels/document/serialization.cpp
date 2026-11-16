#include "levels/document/serialization.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace {
using OrderedJson = nlohmann::ordered_json;

// nlohmann reports syntax errors through exceptions, which web builds
// disable. A SAX pass that accepts everything recovers the error position
// without throwing.
struct SyntaxErrorFinder : nlohmann::json_sax<LevelJson> {
  size_t position = 0;
  std::string message;

  bool null() override { return true; }
  bool boolean(bool) override { return true; }
  bool number_integer(number_integer_t) override { return true; }
  bool number_unsigned(number_unsigned_t) override { return true; }
  bool number_float(number_float_t, const string_t&) override { return true; }
  bool string(string_t&) override { return true; }
  bool binary(binary_t&) override { return true; }
  bool start_object(size_t) override { return true; }
  bool key(string_t&) override { return true; }
  bool end_object() override { return true; }
  bool start_array(size_t) override { return true; }
  bool end_array() override { return true; }
  bool parse_error(size_t errorPosition, const std::string&,
                   const nlohmann::detail::exception& error) override {
    position = errorPosition;
    message = error.what();
    return false;
  }
};

LevelDiagnostic SyntaxError(const std::string& text) {
  SyntaxErrorFinder finder;
  LevelJson::sax_parse(text, &finder);

  LevelDiagnostic diagnostic;
  diagnostic.code = "json_syntax";
  diagnostic.line = 1;
  diagnostic.column = 1;
  const size_t end = std::min(finder.position, text.size());
  for (size_t i = 0; i + 1 < end; i++) {
    if (text[i] == '\n') {
      diagnostic.line++;
      diagnostic.column = 1;
    } else {
      diagnostic.column++;
    }
  }
  // Drop nlohmann's "[json.exception.parse_error.101] " prefix
  const size_t prefixEnd = finder.message.find("] ");
  diagnostic.message =
      "Invalid JSON: " + (prefixEnd == std::string::npos
                              ? finder.message
                              : finder.message.substr(prefixEnd + 2));
  return diagnostic;
}

bool RejectUnknownFields(const LevelJson& source,
                         const std::set<std::string>& known,
                         const LevelFieldErrors& errors) {
  if (!source.is_object()) {
    errors.add("invalid_type", "", "Expected an object");
    return false;
  }
  bool valid = true;
  for (const auto& [key, value] : source.items()) {
    if (known.count(key) == 0) {
      errors.add("unknown_field", "/" + key, "Unknown field \"" + key + "\"");
      valid = false;
    }
  }
  return valid;
}

bool ReadNumber(const LevelJson& source, const std::string& key, double& value,
                const LevelFieldErrors& errors) {
  if (!source.contains(key)) {
    return true;
  }
  if (!source[key].is_number()) {
    errors.add("invalid_type", "/" + key, "\"" + key + "\" must be a number");
    return false;
  }
  value = source[key].get<double>();
  return true;
}

bool ReadNumbers(const LevelJson& source, const std::string& key,
                 double* values, size_t count, const LevelFieldErrors& errors) {
  if (!source.contains(key)) {
    return true;
  }
  const LevelJson& array = source[key];
  const bool valid =
      array.is_array() && array.size() == count &&
      std::all_of(array.begin(), array.end(),
                  [](const LevelJson& item) { return item.is_number(); });
  if (!valid) {
    errors.add("invalid_type", "/" + key,
               "\"" + key + "\" must be an array of " + std::to_string(count) +
                   " numbers");
    return false;
  }
  for (size_t i = 0; i < count; i++) {
    values[i] = array[i].get<double>();
  }
  return true;
}

bool ReadVec3(const LevelJson& source, const std::string& key, LevelVec3& value,
              const LevelFieldErrors& errors) {
  double values[3] = {value.x, value.y, value.z};
  const bool valid = ReadNumbers(source, key, values, 3, errors);
  value = {values[0], values[1], values[2]};
  return valid;
}

bool ReadVec2(const LevelJson& source, const std::string& key, LevelVec2& value,
              const LevelFieldErrors& errors) {
  double values[2] = {value.x, value.y};
  const bool valid = ReadNumbers(source, key, values, 2, errors);
  value = {values[0], values[1]};
  return valid;
}

bool ReadString(const LevelJson& source, const std::string& key,
                std::string& value, bool required,
                const LevelFieldErrors& errors) {
  if (!source.contains(key)) {
    if (required) {
      errors.add("missing_field", "/" + key, "\"" + key + "\" is required");
    }
    return !required;
  }
  if (!source[key].is_string()) {
    errors.add("invalid_type", "/" + key, "\"" + key + "\" must be a string");
    return false;
  }
  value = source[key].get<std::string>();
  return true;
}

LevelFieldErrors Nested(const LevelFieldErrors& errors,
                        const std::string& key) {
  return {errors.errors, errors.objectId, errors.basePath + "/" + key,
          errors.operation};
}

OrderedJson Numbers(std::initializer_list<double> values) {
  OrderedJson array = OrderedJson::array();
  for (const double value : values) {
    array.push_back(value);
  }
  return array;
}

OrderedJson OrderedObject(const LevelObject& object) {
  OrderedJson result;
  result["id"] = object.id;
  result["name"] = object.name;
  result["kind"] = LevelKindName(object.kind);
  result["parent"] =
      object.parent.has_value() ? OrderedJson(*object.parent) : OrderedJson();
  result["order"] = object.order;

  const LevelTransform& transform = object.transform;
  result["transform"]["position"] = Numbers(
      {transform.position.x, transform.position.y, transform.position.z});
  result["transform"]["rotation"] = Numbers(
      {transform.rotation.x, transform.rotation.y, transform.rotation.z});
  result["transform"]["scale"] =
      Numbers({transform.scale.x, transform.scale.y, transform.scale.z});

  const LevelShape& shape = object.shape;
  if (object.kind == LevelKind::Box) {
    result["shape"]["size"] =
        Numbers({shape.size.x, shape.size.y, shape.size.z});
  } else if (object.kind == LevelKind::Plane) {
    result["shape"]["size"] = Numbers({shape.size.x, shape.size.z});
  } else if (object.kind == LevelKind::Sphere) {
    result["shape"]["radius"] = shape.radius;
  } else if (object.kind == LevelKind::Cylinder) {
    result["shape"]["radius"] = shape.radius;
    result["shape"]["height"] = shape.height;
  }

  if (IsPrimitive(object.kind)) {
    const LevelAppearance& appearance = object.appearance;
    result["appearance"]["color"] = appearance.color;
    result["appearance"]["texture"] = appearance.texture.has_value()
                                          ? OrderedJson(*appearance.texture)
                                          : OrderedJson();
    result["appearance"]["tiling"] =
        Numbers({appearance.tiling.x, appearance.tiling.y});
    result["appearance"]["offset"] =
        Numbers({appearance.offset.x, appearance.offset.y});
  }

  if (object.kind == LevelKind::Camera) {
    const LevelCameraSettings& camera = object.camera;
    result["camera"]["projection"] =
        camera.orthographic ? "orthographic" : "perspective";
    result["camera"]["fov"] = camera.fov;
    result["camera"]["size"] = camera.size;
    result["camera"]["near"] = camera.nearPlane;
    result["camera"]["far"] = camera.farPlane;
  }

  result["properties"] = OrderedJson(object.properties);
  return result;
}

}  // namespace

void LevelFieldErrors::add(const std::string& code, const std::string& path,
                           const std::string& message) const {
  LevelDiagnostic diagnostic;
  diagnostic.code = code;
  diagnostic.message = message;
  diagnostic.objectId = objectId;
  diagnostic.path = basePath + path;
  diagnostic.operation = operation;
  errors.push_back(diagnostic);
}

bool ReadTransformFields(const LevelJson& source, LevelTransform& transform,
                         const LevelFieldErrors& errors) {
  bool valid =
      RejectUnknownFields(source, {"position", "rotation", "scale"}, errors);
  if (!source.is_object()) {
    return false;
  }
  valid &= ReadVec3(source, "position", transform.position, errors);
  valid &= ReadVec3(source, "rotation", transform.rotation, errors);
  valid &= ReadVec3(source, "scale", transform.scale, errors);
  return valid;
}

bool ReadShapeFields(const LevelJson& source, LevelKind kind, LevelShape& shape,
                     const LevelFieldErrors& errors) {
  if (!IsPrimitive(kind)) {
    errors.add("invalid_field", "",
               std::string("A ") + LevelKindName(kind) + " has no shape");
    return false;
  }
  if (!source.is_object()) {
    errors.add("invalid_type", "", "Expected an object");
    return false;
  }

  if (kind == LevelKind::Box) {
    return RejectUnknownFields(source, {"size"}, errors) &&
           ReadVec3(source, "size", shape.size, errors);
  }
  if (kind == LevelKind::Plane) {
    LevelVec2 size = {shape.size.x, shape.size.z};
    const bool valid = RejectUnknownFields(source, {"size"}, errors) &&
                       ReadVec2(source, "size", size, errors);
    shape.size.x = size.x;
    shape.size.z = size.y;
    return valid;
  }
  if (kind == LevelKind::Sphere) {
    return RejectUnknownFields(source, {"radius"}, errors) &&
           ReadNumber(source, "radius", shape.radius, errors);
  }
  bool valid = RejectUnknownFields(source, {"radius", "height"}, errors);
  valid &= ReadNumber(source, "radius", shape.radius, errors);
  valid &= ReadNumber(source, "height", shape.height, errors);
  return valid;
}

bool ReadAppearanceFields(const LevelJson& source, LevelAppearance& appearance,
                          const LevelFieldErrors& errors) {
  bool valid = RejectUnknownFields(
      source, {"color", "texture", "tiling", "offset"}, errors);
  if (!source.is_object()) {
    return false;
  }
  valid &= ReadString(source, "color", appearance.color, false, errors);
  if (source.contains("texture")) {
    if (source["texture"].is_null()) {
      appearance.texture.reset();
    } else if (source["texture"].is_string()) {
      appearance.texture = source["texture"].get<std::string>();
    } else {
      errors.add("invalid_type", "/texture",
                 "\"texture\" must be a path string or null");
      valid = false;
    }
  }
  valid &= ReadVec2(source, "tiling", appearance.tiling, errors);
  valid &= ReadVec2(source, "offset", appearance.offset, errors);
  return valid;
}

bool ReadCameraFields(const LevelJson& source, LevelCameraSettings& camera,
                      const LevelFieldErrors& errors) {
  bool valid = RejectUnknownFields(
      source, {"projection", "fov", "size", "near", "far"}, errors);
  if (!source.is_object()) {
    return false;
  }
  if (source.contains("projection")) {
    const LevelJson& projection = source["projection"];
    if (projection == "perspective" || projection == "orthographic") {
      camera.orthographic = projection == "orthographic";
    } else {
      errors.add("invalid_value", "/projection",
                 "\"projection\" must be \"perspective\" or \"orthographic\"");
      valid = false;
    }
  }
  valid &= ReadNumber(source, "fov", camera.fov, errors);
  valid &= ReadNumber(source, "size", camera.size, errors);
  valid &= ReadNumber(source, "near", camera.nearPlane, errors);
  valid &= ReadNumber(source, "far", camera.farPlane, errors);
  return valid;
}

bool ReadObject(const LevelJson& source, LevelObject& object,
                const LevelFieldErrors& errors, bool orderRequired) {
  if (!source.is_object()) {
    errors.add("invalid_type", "", "An object must be a JSON object");
    return false;
  }

  bool valid = ReadString(source, "id", object.id, true, errors);
  const LevelFieldErrors named = {errors.errors, object.id, errors.basePath,
                                  errors.operation};
  valid &= ReadString(source, "name", object.name, true, named);

  std::string kindName;
  if (ReadString(source, "kind", kindName, true, named)) {
    const std::optional<LevelKind> kind = LevelKindFromName(kindName);
    if (kind.has_value()) {
      object.kind = *kind;
    } else {
      named.add("invalid_value", "/kind",
                "Unknown kind \"" + kindName +
                    "\". Use group, marker, box, sphere, cylinder, plane, or "
                    "camera");
      valid = false;
    }
  } else {
    valid = false;
  }

  if (source.contains("parent") && !source["parent"].is_null()) {
    std::string parent;
    valid &= ReadString(source, "parent", parent, false, named);
    object.parent = parent;
  }

  if (source.contains("order") && source["order"].is_number_integer()) {
    object.order = source["order"].get<int>();
  } else if (orderRequired || source.contains("order")) {
    named.add(source.contains("order") ? "invalid_type" : "missing_field",
              "/order", "\"order\" must be an integer");
    valid = false;
  }

  if (source.contains("transform")) {
    valid &= ReadTransformFields(source["transform"], object.transform,
                                 Nested(named, "transform"));
  }
  if (source.contains("shape")) {
    valid &= ReadShapeFields(source["shape"], object.kind, object.shape,
                             Nested(named, "shape"));
  }
  if (source.contains("appearance")) {
    if (IsPrimitive(object.kind)) {
      valid &= ReadAppearanceFields(source["appearance"], object.appearance,
                                    Nested(named, "appearance"));
    } else {
      named.add("invalid_field", "/appearance",
                std::string("A ") + LevelKindName(object.kind) +
                    " has no appearance");
      valid = false;
    }
  }
  if (source.contains("camera")) {
    if (object.kind == LevelKind::Camera) {
      valid &= ReadCameraFields(source["camera"], object.camera,
                                Nested(named, "camera"));
    } else {
      named.add("invalid_field", "/camera",
                "Only camera objects have camera settings");
      valid = false;
    }
  }
  if (source.contains("properties")) {
    if (source["properties"].is_object()) {
      object.properties = source["properties"];
    } else {
      named.add("invalid_type", "/properties",
                "\"properties\" must be an object");
      valid = false;
    }
  }
  return valid;
}

LevelParseResult ParseLevel(const std::string& text) {
  LevelParseResult result;
  const LevelJson document = LevelJson::parse(text, nullptr, false);
  if (document.is_discarded()) {
    result.errors.push_back(SyntaxError(text));
    return result;
  }

  const LevelFieldErrors errors = {result.errors, "", ""};
  if (!document.is_object()) {
    errors.add("invalid_type", "", "A level must be a JSON object");
    return result;
  }
  if (!document.contains("version") ||
      !document["version"].is_number_integer()) {
    errors.add("missing_field", "/version",
               "\"version\" must be an integer; this build reads version " +
                   std::to_string(levelFileVersion));
    return result;
  }
  if (document["version"] != levelFileVersion) {
    errors.add("unsupported_version", "/version",
               "Level version " + document["version"].dump() +
                   " is not supported; this build reads version " +
                   std::to_string(levelFileVersion));
    return result;
  }

  Level level;
  RejectUnknownFields(document, {"version", "camera", "objects"}, errors);
  if (document.contains("camera") && !document["camera"].is_null()) {
    std::string camera;
    if (ReadString(document, "camera", camera, false, errors)) {
      level.camera = camera;
    }
  }
  if (!document.contains("objects") || !document["objects"].is_array()) {
    errors.add("missing_field", "/objects", "\"objects\" must be an array");
    return result;
  }

  const LevelJson& objects = document["objects"];
  for (size_t i = 0; i < objects.size(); i++) {
    LevelObject object;
    const LevelFieldErrors objectErrors = {result.errors, "",
                                           "/objects/" + std::to_string(i)};
    if (objects[i].is_object()) {
      RejectUnknownFields(objects[i],
                          {"id", "name", "kind", "parent", "order", "transform",
                           "shape", "appearance", "camera", "properties"},
                          objectErrors);
    }
    ReadObject(objects[i], object, objectErrors, true);
    level.objects.push_back(object);
  }

  if (result.errors.empty()) {
    result.level = level;
  }
  return result;
}

std::string SerializeLevel(const Level& level) {
  std::multimap<std::string, const LevelObject*> sorted;
  for (const LevelObject& object : level.objects) {
    sorted.insert({object.id, &object});
  }

  OrderedJson document;
  document["version"] = level.version;
  document["camera"] =
      level.camera.has_value() ? OrderedJson(*level.camera) : OrderedJson();
  document["objects"] = OrderedJson::array();
  for (const auto& [id, object] : sorted) {
    document["objects"].push_back(OrderedObject(*object));
  }

  return document.dump(2) + "\n";
}

LevelJson ObjectToJson(const LevelObject& object) {
  return LevelJson(OrderedObject(object));
}
