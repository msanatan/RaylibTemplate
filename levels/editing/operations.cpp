#include "levels/editing/operations.h"

#include <algorithm>
#include <map>
#include <set>

#include "levels/document/level_index.h"
#include "levels/document/serialization.h"
#include "levels/document/transform.h"
#include "levels/document/validation.h"

namespace {
struct Operation {
  Level& level;
  const LevelJson& json;
  LevelDiagnostics& errors;
  int index;
  LevelJson& generatedIds;

  bool requireLevelId(const std::string& id, const std::string& path) const {
    return IsLevelId(id) ||
           fail("invalid_id",
                "\"" + id +
                    "\" is not a UUID. Leave the ID out to have one "
                    "generated, or supply a UUID to reference the object "
                    "later in the same batch",
                id, path);
  }

  bool fail(const std::string& code, const std::string& message,
            const std::string& objectId = "",
            const std::string& path = "") const {
    LevelDiagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.objectId = objectId;
    diagnostic.path = path;
    diagnostic.operation = index;
    errors.push_back(diagnostic);
    return false;
  }

  LevelFieldErrors fieldErrors(const std::string& objectId) const {
    return {errors, objectId, "", index};
  }

  bool allowOnly(const std::set<std::string>& fields) const {
    bool valid = true;
    for (const auto& [key, value] : json.items()) {
      if (key != "op" && fields.count(key) == 0) {
        valid = fail("unknown_argument", "Unknown argument \"" + key + "\"", "",
                     "/" + key);
      }
    }
    return valid;
  }

  bool readString(const std::string& key, std::string& value) const {
    if (!json.contains(key) || !json[key].is_string()) {
      return fail("invalid_argument", "\"" + key + "\" must be a string", "",
                  "/" + key);
    }
    value = json[key].get<std::string>();
    return true;
  }

  LevelObject* target() const {
    std::string id;
    if (!readString("id", id)) {
      return nullptr;
    }
    LevelObject* object = level.find(id);
    if (object == nullptr) {
      fail("unknown_object", "Object \"" + id + "\" does not exist", id);
    }
    return object;
  }

  LevelJson fields() const {
    LevelJson result = json;
    result.erase("op");
    result.erase("id");
    return result;
  }

  bool readParent(std::optional<std::string>& parent) const {
    if (json["parent"].is_null()) {
      parent.reset();
      return true;
    }
    if (!json["parent"].is_string()) {
      return fail("invalid_argument", "\"parent\" must be an object ID or null",
                  "", "/parent");
    }
    parent = json["parent"].get<std::string>();
    if (level.find(*parent) == nullptr) {
      return fail("unknown_parent", "Parent \"" + *parent + "\" does not exist",
                  "", "/parent");
    }
    return true;
  }

  bool readIndex(const std::optional<std::string>& parent,
                 const std::string& movingId, int& insertAt) const {
    int siblings = 0;
    for (const LevelObject& object : level.objects) {
      siblings += object.parent == parent && object.id != movingId ? 1 : 0;
    }
    if (!json.contains("index")) {
      insertAt = siblings;
      return true;
    }
    if (!json["index"].is_number_integer() || json["index"] < 0 ||
        json["index"] > siblings) {
      return fail(
          "invalid_index",
          "\"index\" must be an integer from 0 to " + std::to_string(siblings),
          movingId, "/index");
    }
    insertAt = json["index"].get<int>();
    return true;
  }
};

void DetachFromSiblings(Level& level, const LevelObject& object) {
  for (LevelObject& sibling : level.objects) {
    if (sibling.parent == object.parent && sibling.id != object.id &&
        sibling.order > object.order) {
      sibling.order--;
    }
  }
}

void AttachToSiblings(Level& level, LevelObject& object,
                      const std::optional<std::string>& parent, int index) {
  for (LevelObject& sibling : level.objects) {
    if (sibling.parent == parent && sibling.id != object.id &&
        sibling.order >= index) {
      sibling.order++;
    }
  }
  object.parent = parent;
  object.order = index;
}

bool Create(const Operation& operation) {
  if (!operation.allowOnly({"id", "name", "kind", "parent", "index",
                            "transform", "shape", "appearance", "camera",
                            "properties"})) {
    return false;
  }

  LevelJson source = operation.json;
  source.erase("op");
  source.erase("index");
  source.erase("parent");
  const bool generated = !source.contains("id");
  if (generated) {
    source["id"] = NewLevelId();
  }
  LevelObject object;
  if (!ReadObject(source, object, operation.fieldErrors(""), false) ||
      !operation.requireLevelId(object.id, "/id")) {
    return false;
  }
  if (generated) {
    operation.generatedIds.push_back(
        {{"operation", operation.index}, {"id", object.id}});
  }
  if (operation.level.find(object.id) != nullptr) {
    return operation.fail(
        "duplicate_id",
        "An object with the ID \"" + object.id + "\" already exists", object.id,
        "/id");
  }

  std::optional<std::string> parent;
  int index = 0;
  if ((operation.json.contains("parent") && !operation.readParent(parent)) ||
      !operation.readIndex(parent, object.id, index)) {
    return false;
  }
  AttachToSiblings(operation.level, object, parent, index);
  operation.level.objects.push_back(object);
  return true;
}

bool Rename(const Operation& operation) {
  LevelObject* object = operation.target();
  return object != nullptr && operation.allowOnly({"id", "name"}) &&
         operation.readString("name", object->name);
}

bool SetTransform(const Operation& operation) {
  LevelObject* object = operation.target();
  return object != nullptr &&
         ReadTransformFields(operation.fields(), object->transform,
                             operation.fieldErrors(object->id));
}

bool SetShape(const Operation& operation) {
  LevelObject* object = operation.target();
  return object != nullptr &&
         ReadShapeFields(operation.fields(), object->kind, object->shape,
                         operation.fieldErrors(object->id));
}

bool SetAppearance(const Operation& operation) {
  LevelObject* object = operation.target();
  if (object == nullptr) {
    return false;
  }
  if (!IsPrimitive(object->kind)) {
    return operation.fail(
        "invalid_operation",
        std::string("A ") + LevelKindName(object->kind) + " has no appearance",
        object->id);
  }
  return ReadAppearanceFields(operation.fields(), object->appearance,
                              operation.fieldErrors(object->id));
}

bool SetCamera(const Operation& operation) {
  LevelObject* object = operation.target();
  if (object == nullptr) {
    return false;
  }
  if (object->kind != LevelKind::Camera) {
    return operation.fail("invalid_operation",
                          "Only camera objects have camera settings",
                          object->id);
  }
  return ReadCameraFields(operation.fields(), object->camera,
                          operation.fieldErrors(object->id));
}

bool SetLevelCamera(const Operation& operation) {
  if (!operation.allowOnly({"camera"})) {
    return false;
  }
  if (!operation.json.contains("camera")) {
    return operation.fail("invalid_argument",
                          "\"camera\" must be a camera object ID or null", "",
                          "/camera");
  }
  if (operation.json["camera"].is_null()) {
    operation.level.camera.reset();
    return true;
  }
  std::string camera;
  if (!operation.readString("camera", camera)) {
    return false;
  }
  operation.level.camera = camera;
  return true;
}

bool ParsePointer(const Operation& operation, const std::string& objectId,
                  std::vector<std::string>& tokens) {
  std::string path;
  if (!operation.readString("path", path)) {
    return false;
  }
  if (path.empty() || path[0] != '/') {
    return operation.fail(
        "invalid_path",
        "\"path\" must be a JSON Pointer inside the object's properties, such "
        "as \"/door/locked\"",
        objectId, "/path");
  }
  try {
    LevelJson::json_pointer pointer(path);
    while (!pointer.empty()) {
      tokens.push_back(pointer.back());
      pointer.pop_back();
    }
    std::reverse(tokens.begin(), tokens.end());
  } catch (const LevelJson::exception& error) {
    return operation.fail("invalid_path", error.what(), objectId, "/path");
  }
  return true;
}

std::optional<size_t> ArrayIndex(const LevelJson& array,
                                 const std::string& token) {
  if (token.empty() || token.size() > 9 ||
      (token.size() > 1 && token[0] == '0') ||
      !std::all_of(token.begin(), token.end(),
                   [](unsigned char c) { return std::isdigit(c) != 0; })) {
    return std::nullopt;
  }
  const size_t index = (size_t)std::stoul(token);
  return index < array.size() ? std::optional<size_t>(index) : std::nullopt;
}

bool SetProperty(const Operation& operation) {
  LevelObject* object = operation.target();
  std::vector<std::string> tokens;
  if (object == nullptr || !operation.allowOnly({"id", "path", "value"}) ||
      !ParsePointer(operation, object->id, tokens)) {
    return false;
  }
  if (!operation.json.contains("value")) {
    return operation.fail("invalid_argument", "\"value\" is required",
                          object->id, "/value");
  }

  const auto incompatible = [&](const std::string& token) {
    return operation.fail(
        "incompatible_path",
        "Cannot set \"" + operation.json["path"].get<std::string>() + "\": \"" +
            token +
            "\" is not a key of an object or an existing array index. Arrays "
            "do not grow implicitly; set the whole array instead",
        object->id, "/path");
  };

  LevelJson* current = &object->properties;
  for (size_t i = 0; i < tokens.size(); i++) {
    const bool last = i + 1 == tokens.size();
    LevelJson* next = nullptr;
    if (current->is_object()) {
      if (!last && !current->contains(tokens[i])) {
        (*current)[tokens[i]] = LevelJson::object();
      }
      next = &(*current)[tokens[i]];
    } else if (current->is_array()) {
      const std::optional<size_t> index = ArrayIndex(*current, tokens[i]);
      if (!index.has_value()) {
        return incompatible(tokens[i]);
      }
      next = &(*current)[*index];
    } else {
      return incompatible(tokens[i]);
    }
    current = next;
  }
  *current = operation.json["value"];
  return true;
}

bool RemoveProperty(const Operation& operation) {
  LevelObject* object = operation.target();
  std::vector<std::string> tokens;
  if (object == nullptr || !operation.allowOnly({"id", "path"}) ||
      !ParsePointer(operation, object->id, tokens)) {
    return false;
  }

  LevelJson* current = &object->properties;
  for (size_t i = 0; i < tokens.size(); i++) {
    const bool last = i + 1 == tokens.size();
    const std::optional<size_t> index =
        current->is_array() ? ArrayIndex(*current, tokens[i]) : std::nullopt;
    const bool exists =
        (current->is_object() && current->contains(tokens[i])) ||
        index.has_value();
    if (!exists) {
      return operation.fail("unknown_property",
                            "Property \"" +
                                operation.json["path"].get<std::string>() +
                                "\" does not exist",
                            object->id, "/path");
    }
    if (!last) {
      current =
          index.has_value() ? &(*current)[*index] : &(*current)[tokens[i]];
    } else if (index.has_value()) {
      current->erase(current->begin() + (std::ptrdiff_t)*index);
    } else {
      current->erase(tokens[i]);
    }
  }
  return true;
}

bool Reparent(const Operation& operation) {
  LevelObject* object = operation.target();
  if (object == nullptr ||
      !operation.allowOnly({"id", "parent", "index", "mode"})) {
    return false;
  }
  std::string mode;
  if (!operation.json.contains("parent") ||
      !operation.readString("mode", mode)) {
    return operation.fail(
        "invalid_argument",
        "\"parent\" (an object ID or null) and \"mode\" (\"keep-world\" or "
        "\"keep-local\") are required",
        object->id);
  }
  if (mode != "keep-world" && mode != "keep-local") {
    return operation.fail("invalid_argument",
                          "\"mode\" must be \"keep-world\" or \"keep-local\"",
                          object->id, "/mode");
  }

  std::optional<std::string> parent;
  int index = 0;
  if (!operation.readParent(parent)) {
    return false;
  }
  if (parent.has_value() &&
      (*parent == object->id ||
       LevelIndex(operation.level).isDescendant(*parent, object->id))) {
    return operation.fail("hierarchy_cycle",
                          "An object cannot be moved under itself or one of "
                          "its descendants",
                          object->id, "/parent");
  }
  if (!operation.readIndex(parent, object->id, index)) {
    return false;
  }

  if (mode == "keep-world") {
    const LevelIndex hierarchy(operation.level);
    const LevelMatrix world = hierarchy.world(object->id);
    const LevelMatrix parentWorld =
        parent.has_value() ? hierarchy.world(*parent) : LevelMatrix();
    const std::optional<LevelTransform> local =
        DecomposeMatrix(Multiply(Inverse(parentWorld), world));
    if (!local.has_value()) {
      return operation.fail(
          "unsupported_shear",
          "Keeping the world placement under this parent would need shear, "
          "which position, rotation, and scale cannot store. Use "
          "\"keep-local\", or make the scale of the rotated ancestors uniform",
          object->id);
    }
    object->transform = *local;
  }

  DetachFromSiblings(operation.level, *object);
  AttachToSiblings(operation.level, *object, parent, index);
  return true;
}

bool Reorder(const Operation& operation) {
  LevelObject* object = operation.target();
  int index = 0;
  if (object == nullptr || !operation.allowOnly({"id", "index"})) {
    return false;
  }
  if (!operation.json.contains("index")) {
    return operation.fail("invalid_argument", "\"index\" is required",
                          object->id, "/index");
  }
  if (!operation.readIndex(object->parent, object->id, index)) {
    return false;
  }
  const std::optional<std::string> parent = object->parent;
  DetachFromSiblings(operation.level, *object);
  AttachToSiblings(operation.level, *object, parent, index);
  return true;
}

bool Duplicate(const Operation& operation) {
  const LevelObject* source = operation.target();
  if (source == nullptr ||
      !operation.allowOnly({"id", "ids", "parent", "index"})) {
    return false;
  }
  const std::string sourceId = source->id;

  std::map<std::string, std::string> newIds;
  if (operation.json.contains("ids")) {
    if (!operation.json["ids"].is_object()) {
      return operation.fail("invalid_argument",
                            "\"ids\" must map old IDs to new UUIDs", sourceId,
                            "/ids");
    }
    for (const auto& [oldId, newId] : operation.json["ids"].items()) {
      if (!newId.is_string()) {
        return operation.fail("invalid_argument",
                              "Replacement IDs must be strings", sourceId,
                              "/ids/" + oldId);
      }
      if (!operation.requireLevelId(newId.get<std::string>(),
                                    "/ids/" + oldId)) {
        return false;
      }
      newIds[oldId] = newId.get<std::string>();
    }
  }

  std::vector<LevelObject> copies;
  std::set<std::string> created;
  LevelJson assigned = LevelJson::object();
  for (const LevelObject* original :
       LevelIndex(operation.level).subtree(sourceId)) {
    const auto supplied = newIds.find(original->id);
    const std::string newId =
        supplied == newIds.end() ? NewLevelId() : supplied->second;
    if (operation.level.find(newId) != nullptr ||
        !created.insert(newId).second) {
      return operation.fail("duplicate_id",
                            "The new ID \"" + newId + "\" is already in use",
                            original->id, "/ids/" + original->id);
    }
    newIds[original->id] = newId;
    assigned[original->id] = newId;
    copies.push_back(*original);
  }
  if (newIds.size() != copies.size()) {
    return operation.fail(
        "invalid_argument",
        "\"ids\" names objects outside the duplicated subtree", sourceId,
        "/ids");
  }

  std::optional<std::string> parent = source->parent;
  int index = source->order + 1;
  if (operation.json.contains("parent") && !operation.readParent(parent)) {
    return false;
  }
  if ((operation.json.contains("parent") || operation.json.contains("index")) &&
      !operation.readIndex(parent, "", index)) {
    return false;
  }

  for (LevelObject& copy : copies) {
    const bool isRoot = copy.id == sourceId;
    copy.id = newIds[copy.id];
    if (!isRoot && copy.parent.has_value()) {
      copy.parent = newIds[*copy.parent];
    }
    ForEachReference(copy.properties, [&](std::string& id, const std::string&) {
      const auto remapped = newIds.find(id);
      if (remapped != newIds.end()) {
        id = remapped->second;
      }
    });
  }
  operation.generatedIds.push_back(
      {{"operation", operation.index}, {"ids", assigned}});
  AttachToSiblings(operation.level, copies.front(), parent, index);
  operation.level.objects.insert(operation.level.objects.end(), copies.begin(),
                                 copies.end());
  return true;
}

bool Delete(const Operation& operation) {
  const LevelObject* object = operation.target();
  if (object == nullptr || !operation.allowOnly({"id", "subtree"})) {
    return false;
  }
  if (operation.json.contains("subtree") &&
      !operation.json["subtree"].is_boolean()) {
    return operation.fail("invalid_argument", "\"subtree\" must be a boolean",
                          object->id, "/subtree");
  }

  std::set<std::string> doomed;
  for (const LevelObject* member :
       LevelIndex(operation.level).subtree(object->id)) {
    doomed.insert(member->id);
  }
  if (doomed.size() > 1 && !operation.json.value("subtree", false)) {
    return operation.fail("has_children",
                          "\"" + object->id + "\" has " +
                              std::to_string(doomed.size() - 1) +
                              " descendants. Pass \"subtree\": true to delete "
                              "them too, or reparent them first",
                          object->id);
  }

  DetachFromSiblings(operation.level, *object);
  std::erase_if(operation.level.objects, [&](const LevelObject& candidate) {
    return doomed.count(candidate.id) > 0;
  });
  return true;
}

using Handler = bool (*)(const Operation&);
const std::map<std::string, Handler>& Handlers() {
  static const std::map<std::string, Handler> handlers = {
      {"create", Create},
      {"rename", Rename},
      {"set-transform", SetTransform},
      {"set-shape", SetShape},
      {"set-appearance", SetAppearance},
      {"set-camera", SetCamera},
      {"set-level-camera", SetLevelCamera},
      {"set-property", SetProperty},
      {"remove-property", RemoveProperty},
      {"reparent", Reparent},
      {"reorder", Reorder},
      {"duplicate", Duplicate},
      {"delete", Delete},
  };
  return handlers;
}
}  // namespace

LevelJson DescribeLevelChanges(const Level& before, const Level& after,
                               std::vector<std::string>& affectedIds) {
  std::set<std::string> ids;
  for (const LevelObject& object : before.objects) {
    ids.insert(object.id);
  }
  for (const LevelObject& object : after.objects) {
    ids.insert(object.id);
  }

  const LevelIndex beforeIndex(before);
  const LevelIndex afterIndex(after);
  LevelJson changes = LevelJson::array();
  for (const std::string& id : ids) {
    const LevelObject* old = beforeIndex.find(id);
    const LevelObject* current = afterIndex.find(id);
    if (old != nullptr && current != nullptr && *old == *current) {
      continue;
    }

    LevelJson change = {{"id", id}};
    if (old == nullptr) {
      change["change"] = "created";
      change["fields"] = {{{"path", ""}, {"value", ObjectToJson(*current)}}};
    } else if (current == nullptr) {
      change["change"] = "deleted";
    } else {
      change["change"] = "updated";
      change["fields"] = LevelJson::array();
      for (const LevelJson& patch :
           LevelJson::diff(ObjectToJson(*old), ObjectToJson(*current))) {
        LevelJson field = {{"path", patch["path"]}};
        if (patch.contains("value")) {
          field["value"] = patch["value"];
        } else {
          field["removed"] = true;
        }
        change["fields"].push_back(field);
      }
    }
    changes.push_back(change);
    affectedIds.push_back(id);
  }

  if (before.camera != after.camera) {
    changes.push_back(
        {{"id", nullptr},
         {"change", "updated"},
         {"fields",
          {{{"path", "/camera"},
            {"value", after.camera.has_value() ? LevelJson(*after.camera)
                                               : LevelJson()}}}}});
  }
  return changes;
}

LevelEditResult ApplyLevelOperations(const Level& level,
                                     const LevelJson& request) {
  LevelEditResult result;
  result.level = level;

  const auto malformed = [&](const std::string& message) {
    LevelDiagnostic diagnostic;
    diagnostic.code = "invalid_request";
    diagnostic.message = message;
    result.errors.push_back(diagnostic);
    return result;
  };
  if (!request.is_object() || !request.contains("operations") ||
      !request["operations"].is_array() || request.size() != 1) {
    return malformed(
        "The request must be an object of the form {\"operations\": [...]}");
  }

  const LevelJson& operations = request["operations"];
  for (size_t i = 0; i < operations.size(); i++) {
    const Operation operation = {result.level, operations[i], result.errors,
                                 (int)i, result.generatedIds};
    if (!operations[i].is_object() || !operations[i].contains("op") ||
        !operations[i]["op"].is_string()) {
      operation.fail("invalid_request",
                     "Each operation must be an object with an \"op\" name");
      return result;
    }
    const auto handler =
        Handlers().find(operations[i]["op"].get<std::string>());
    if (handler == Handlers().end()) {
      operation.fail("unknown_operation",
                     "Unknown operation " + operations[i]["op"].dump(), "",
                     "/op");
      return result;
    }
    if (!handler->second(operation)) {
      if (result.errors.empty()) {
        operation.fail("invalid_operation",
                       "Operation failed without a diagnostic");
      }
      return result;
    }
  }

  result.errors = ValidateLevel(result.level);
  if (!result.errors.empty()) {
    return result;
  }
  result.ok = true;
  result.changed = result.level != level;
  return result;
}

const LevelJson& LevelOperationsSchema() {
  static const LevelJson schema = LevelJson::parse(R"json({
    "type": "object",
    "required": ["operations"],
    "additionalProperties": false,
    "description": "Operations run in order on a copy of the level. The whole result is validated once; any failure changes nothing.",
    "properties": {"operations": {"type": "array", "items": {"oneOf": [
      {"title": "create", "description": "Add a fully initialised object. Omit id to have a UUID generated (reported in generated_ids); supply your own UUID only to reference the object later in the same batch. Omitted fields default to: parent null, index appended last, identity transform, unit shape (box size [1,1,1], plane size [1,1], sphere radius 0.5, cylinder radius 0.5 height 1), appearance white with no texture, tiling [1,1], offset [0,0], camera perspective fov 45 size 10 near 0.1 far 1000, properties {}.",
       "required": ["op", "name", "kind"],
       "properties": {"op": {"const": "create"}, "id": {"type": "string"}, "name": {"type": "string"},
         "kind": {"enum": ["group", "marker", "box", "sphere", "cylinder", "plane", "camera"]},
         "parent": {"type": ["string", "null"]}, "index": {"type": "integer", "minimum": 0},
         "transform": {"$ref": "#/definitions/transform"}, "shape": {"$ref": "#/definitions/shape"},
         "appearance": {"$ref": "#/definitions/appearance"}, "camera": {"$ref": "#/definitions/camera"},
         "properties": {"type": "object"}}},
      {"title": "rename", "required": ["op", "id", "name"],
       "properties": {"op": {"const": "rename"}, "id": {"type": "string"}, "name": {"type": "string"}}},
      {"title": "set-transform", "description": "Updates only the supplied local transform fields.",
       "required": ["op", "id"],
       "properties": {"op": {"const": "set-transform"}, "id": {"type": "string"},
         "position": {"$ref": "#/definitions/vec3"}, "rotation": {"$ref": "#/definitions/vec3"}, "scale": {"$ref": "#/definitions/vec3"}}},
      {"title": "set-shape", "description": "Updates the supplied dimensions of a primitive: size for box [x,y,z] and plane [x,z], radius for sphere, radius and height for cylinder.",
       "required": ["op", "id"],
       "properties": {"op": {"const": "set-shape"}, "id": {"type": "string"},
         "size": {"type": "array"}, "radius": {"type": "number"}, "height": {"type": "number"}}},
      {"title": "set-appearance", "required": ["op", "id"],
       "properties": {"op": {"const": "set-appearance"}, "id": {"type": "string"},
         "color": {"type": "string"}, "texture": {"type": ["string", "null"]},
         "tiling": {"$ref": "#/definitions/vec2"}, "offset": {"$ref": "#/definitions/vec2"}}},
      {"title": "set-camera", "required": ["op", "id"],
       "properties": {"op": {"const": "set-camera"}, "id": {"type": "string"},
         "projection": {"enum": ["perspective", "orthographic"]}, "fov": {"type": "number"},
         "size": {"type": "number"}, "near": {"type": "number"}, "far": {"type": "number"}}},
      {"title": "set-level-camera", "description": "Designates the camera that supplies the game's initial view. null restores the game-defined fallback.",
       "required": ["op", "camera"],
       "properties": {"op": {"const": "set-level-camera"}, "camera": {"type": ["string", "null"]}}},
      {"title": "set-property", "description": "Sets a value inside the object's custom properties at a JSON Pointer. Missing objects along the path are created. Fails when the path crosses a non-container value or names an array index that does not exist. Use {\"$ref\": \"<object id>\"} for a reference to another object.",
       "required": ["op", "id", "path", "value"],
       "properties": {"op": {"const": "set-property"}, "id": {"type": "string"}, "path": {"type": "string"}, "value": {}}},
      {"title": "remove-property", "description": "Removes an existing property. A missing path is an error.",
       "required": ["op", "id", "path"],
       "properties": {"op": {"const": "remove-property"}, "id": {"type": "string"}, "path": {"type": "string"}}},
      {"title": "reparent", "description": "keep-world preserves the world placement and fails with unsupported_shear when the new parent makes that impossible; keep-local keeps the local transform values. index defaults to last.",
       "required": ["op", "id", "parent", "mode"],
       "properties": {"op": {"const": "reparent"}, "id": {"type": "string"}, "parent": {"type": ["string", "null"]},
         "index": {"type": "integer", "minimum": 0}, "mode": {"enum": ["keep-world", "keep-local"]}}},
      {"title": "reorder", "description": "Moves the object to a zero-based position among its siblings.",
       "required": ["op", "id", "index"],
       "properties": {"op": {"const": "reorder"}, "id": {"type": "string"}, "index": {"type": "integer", "minimum": 0}}},
      {"title": "duplicate", "description": "Copies an object and its descendants. Every copy gets a generated UUID, reported in generated_ids as an old-to-new map; ids may supply UUIDs for some or all of them. References between copied objects follow the copies. The copy is placed after the original unless parent or index is given.",
       "required": ["op", "id"],
       "properties": {"op": {"const": "duplicate"}, "id": {"type": "string"},
         "ids": {"type": "object", "additionalProperties": {"type": "string"}},
         "parent": {"type": ["string", "null"]}, "index": {"type": "integer", "minimum": 0}}},
      {"title": "delete", "description": "subtree: true is required when the object has children. Fails when remaining objects or the level camera still reference a deleted object.",
       "required": ["op", "id"],
       "properties": {"op": {"const": "delete"}, "id": {"type": "string"}, "subtree": {"type": "boolean"}}}
    ]}}},
    "definitions": {
      "vec3": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
      "vec2": {"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2},
      "transform": {"type": "object", "additionalProperties": false, "properties": {
        "position": {"$ref": "#/definitions/vec3"}, "rotation": {"$ref": "#/definitions/vec3"}, "scale": {"$ref": "#/definitions/vec3"}}},
      "shape": {"type": "object", "additionalProperties": false, "properties": {
        "size": {"type": "array"}, "radius": {"type": "number"}, "height": {"type": "number"}}},
      "appearance": {"type": "object", "additionalProperties": false, "properties": {
        "color": {"type": "string", "pattern": "^#[0-9A-Fa-f]{6}$"}, "texture": {"type": ["string", "null"]},
        "tiling": {"$ref": "#/definitions/vec2"}, "offset": {"$ref": "#/definitions/vec2"}}},
      "camera": {"type": "object", "additionalProperties": false, "properties": {
        "projection": {"enum": ["perspective", "orthographic"]}, "fov": {"type": "number"},
        "size": {"type": "number"}, "near": {"type": "number"}, "far": {"type": "number"}}}
    }
  })json");
  return schema;
}
