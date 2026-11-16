#include <map>

#include "levels/document/level_index.h"
#include "levels/document/serialization.h"
#include "levels/document/transform.h"
#include "levels/document/validation.h"
#include "levels/editing/commands.h"
#include "levels/editing/operations.h"

namespace {
const size_t maximumReportedChanges = 50;

std::string Scope(const CommandTarget& target, const std::string& query) {
  const DocumentState document = target.document();
  return std::string(target.kind()) + "|" + target.session() + "|" +
         document.id + "|" + document.path + "|" + document.revision + "|" +
         query;
}

// Marks values too long to return whole, so nothing is cut silently
LevelJson Bounded(const LevelJson& value, bool& truncated) {
  const std::string text = value.dump();
  if (text.size() <= maximumValueLength) {
    return value;
  }
  truncated = true;
  return {{"truncated", true},
          {"length", text.size()},
          {"preview", text.substr(0, maximumValueLength)}};
}

LevelJson TransformJson(const LevelTransform& transform) {
  return {
      {"position",
       {transform.position.x, transform.position.y, transform.position.z}},
      {"rotation",
       {transform.rotation.x, transform.rotation.y, transform.rotation.z}},
      {"scale", {transform.scale.x, transform.scale.y, transform.scale.z}},
  };
}

LevelJson JsonSchema(const std::vector<CommandParam>& params) {
  static const std::map<std::string, LevelJson> types = {
      {"string", {{"type", "string"}}},
      {"integer", {{"type", "integer"}}},
      {"boolean", {{"type", "boolean"}}},
      {"strings", {{"type", "array"}, {"items", {{"type", "string"}}}}},
      {"json", LevelJson::object()},
  };

  LevelJson schema = {{"type", "object"},
                      {"properties", LevelJson::object()},
                      {"required", LevelJson::array()}};
  for (const CommandParam& param : params) {
    LevelJson property = types.at(param.type);
    property["description"] = param.description;
    schema["properties"][param.name] = property;
    if (param.required) {
      schema["required"].push_back(param.name);
    }
  }
  return schema;
}

LevelJson ListCommands(const CommandRegistry& registry) {
  LevelJson commands = LevelJson::array();
  for (const Command& command : registry.commands()) {
    LevelJson targets = LevelJson::array();
    if (command.supportsFile) {
      targets.push_back("file");
    }
    if (command.supportsEditor) {
      targets.push_back("editor");
    }
    commands.push_back({{"name", command.name},
                        {"description", command.description},
                        {"targets", targets}});
  }
  return {{"commands", commands}};
}

LevelJson DescribeCommand(const CommandRegistry& registry,
                          const LevelJson& args, const std::string& target) {
  const std::string name = args["name"].get<std::string>();
  const Command* command = registry.find(name, target);
  if (command == nullptr) {
    throw CommandFailure{
        CommandExit::Invalid, "unknown_command",
        "Unknown command \"" + name + "\". Run `commands` to list them"};
  }

  std::vector<CommandParam> params = command->params;
  if (command->mutates) {
    params.push_back({"expect", "string", true,
                      "The revision last returned for this document"});
    params.push_back({"document", "string", false,
                      "The document ID; required with --editor"});
  }
  LevelJson description = {
      {"name", command->name},
      {"description", command->description},
      {"side_effects", command->hasSideEffects},
      {"arguments", JsonSchema(params)},
      {"result",
       {{"common",
         {{"ok", "Whether the command succeeded"},
          {"code", "Stable result or error code"},
          {"changed", "Whether the command changed state"},
          {"affected_ids", "Object IDs created, changed, or deleted"},
          {"warnings", "Warnings; may include missing assets"},
          {"errors", "Diagnostics with code, message, and context"},
          {"exit_code",
           "0 success, 2 invalid, 3 conflict, 4 I/O, 5 noncanonical"},
          {"command", "Resolved command name, or null"},
          {"target", "Target kind and optional editor session"},
          {"document",
           "Open document path, revision, dirty state, and optional ID; or "
           "null"},
          {"request_id", "Included when supplied"},
          {"category", "Failure category, on errors"},
          {"message", "Failure explanation, on errors"},
          {"completed",
           "Optional completion marker; null if no action completed"},
          {"total", "Total records for paginated object lists and validation"},
          {"has_more", "Whether another page exists, for paginated results"},
          {"next_cursor",
           "Pass as --cursor for the next page, when provided"}}},
        {"payload", command->resultDescription}}}};
  if (!command->positional.empty()) {
    description["positional"] = command->positional;
  }
  if (command->name == "apply") {
    description["arguments"]["properties"]["ops"] = LevelOperationsSchema();
  }
  return {{"describe", description}};
}

LevelJson InspectObject(const Level& level, const LevelJson& args) {
  const std::string id = args["id"].get<std::string>();
  const LevelObject* object = level.find(id);
  if (object == nullptr) {
    throw CommandFailure{CommandExit::Invalid, "unknown_object",
                         "Object \"" + id + "\" does not exist"};
  }

  bool truncated = false;
  if (args.contains("path")) {
    const std::string path = args["path"].get<std::string>();
    LevelJson::json_pointer pointer;
    bool wellFormed = true;
    try {
      pointer = LevelJson::json_pointer(path);
    } catch (const LevelJson::exception&) {
      wellFormed = false;
    }
    // contains() is checked first because resolving a missing pointer throws
    if (!wellFormed || !object->properties.contains(pointer)) {
      throw CommandFailure{
          CommandExit::Invalid, "unknown_property",
          "\"" + id + "\" has no property at \"" + path + "\""};
    }
    LevelJson result = {{"id", id}, {"path", path}};
    result["value"] = Bounded(object->properties[pointer], truncated);
    result["truncated"] = truncated;
    return result;
  }

  LevelJson fields = ObjectToJson(*object);
  fields["properties"] = Bounded(object->properties, truncated);
  const LevelIndex index(level);
  const std::optional<LevelTransform> world = index.worldTransform(id);
  LevelJson result = {
      {"object", fields},
      {"local_transform", TransformJson(object->transform)},
      {"world_transform",
       world.has_value() ? TransformJson(*world) : LevelJson()},
      {"child_count", index.children(id).size()},
      {"truncated", truncated},
  };
  if (!world.has_value()) {
    result["warnings"] = {
        "A rotated, non-uniformly scaled ancestor shears this object, so its "
        "world transform has no position, rotation, and scale form"};
  }
  return result;
}

LevelJson Inspect(CommandTarget& target, const LevelJson& args) {
  const Level& level = RequireLevel(target);
  if (args.contains("id")) {
    return InspectObject(level, args);
  }
  if (args.contains("path")) {
    throw CommandFailure{CommandExit::Invalid, "missing_argument",
                         "--path needs --id to name the object"};
  }

  LevelJson kinds = LevelJson::object();
  for (const LevelObject& object : level.objects) {
    const std::string kind = LevelKindName(object.kind);
    kinds[kind] = kinds.value(kind, 0) + 1;
  }
  return {{"summary",
           {{"version", level.version},
            {"camera",
             level.camera.has_value() ? LevelJson(*level.camera) : LevelJson()},
            {"object_count", level.objects.size()},
            {"root_count", LevelIndex(level).children(std::nullopt).size()},
            {"kinds", kinds}}}};
}

LevelJson ListObjects(CommandTarget& target, const LevelJson& args) {
  const Level& level = RequireLevel(target);
  if (args.contains("parent") + args.contains("subtree") +
          args.value("roots", false) >
      1) {
    throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                         "Use only one of --parent, --subtree, and --roots"};
  }
  for (const char* filter : {"parent", "subtree"}) {
    if (args.contains(filter) &&
        level.find(args[filter].get<std::string>()) == nullptr) {
      throw CommandFailure{CommandExit::Invalid, "unknown_object",
                           "Object " + args[filter].dump() + " does not exist"};
    }
  }
  std::optional<LevelKind> kind;
  if (args.contains("kind")) {
    kind = LevelKindFromName(args["kind"].get<std::string>());
    if (!kind.has_value()) {
      throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                           "Unknown kind " + args["kind"].dump()};
    }
  }

  const LevelIndex index(level);
  std::vector<const LevelObject*> objects;
  if (args.contains("parent")) {
    objects = index.children(args["parent"].get<std::string>());
  } else if (args.value("roots", false)) {
    objects = index.children(std::nullopt);
  } else if (args.contains("subtree")) {
    objects = index.subtree(args["subtree"].get<std::string>());
  } else {
    objects = index.hierarchy();
  }
  if (kind.has_value()) {
    std::erase_if(objects, [&](const LevelObject* object) {
      return object->kind != *kind;
    });
  }

  LevelJson filters = args;
  filters.erase("cursor");
  filters.erase("limit");
  const std::string scope = Scope(target, "objects list" + filters.dump());
  const CommandPage page = ReadPage(args, scope);

  LevelJson list = LevelJson::array();
  for (size_t i = page.offset;
       i < objects.size() && i < page.offset + page.limit; i++) {
    const LevelObject& object = *objects[i];
    list.push_back(
        {{"id", object.id},
         {"name", object.name},
         {"kind", LevelKindName(object.kind)},
         {"parent",
          object.parent.has_value() ? LevelJson(*object.parent) : LevelJson()},
         {"order", object.order}});
  }
  LevelJson result = {{"objects", list}};
  WritePage(result, page, objects.size(), scope);
  return result;
}

LevelJson Validate(CommandTarget& target, const LevelJson& args) {
  if (!target.document().open) {
    RequireLevel(target);
  }
  LevelDiagnostics diagnostics = target.loadErrors();
  if (const Level* level = target.document().level) {
    const LevelDiagnostics structure = ValidateLevel(*level);
    const LevelDiagnostics assets =
        ValidateLevelAssets(*level, target.assetsDirectory());
    diagnostics.insert(diagnostics.end(), structure.begin(), structure.end());
    diagnostics.insert(diagnostics.end(), assets.begin(), assets.end());
  }

  LevelJson all = LevelJson::array();
  size_t errors = 0;
  for (const LevelDiagnostic& diagnostic : diagnostics) {
    all.push_back(DiagnosticToJson(diagnostic));
    errors += diagnostic.warning ? 0 : 1;
  }

  // The audit's own content is part of the scope, so later pages can only
  // continue the audit they started from, asset checks included
  const std::string scope = Scope(target, "validate" + all.dump());
  const CommandPage page = ReadPage(args, scope);
  LevelJson shown = LevelJson::array();
  for (size_t i = page.offset; i < all.size() && i < page.offset + page.limit;
       i++) {
    shown.push_back(all[i]);
  }

  LevelJson result = {{"ok", errors == 0},
                      {"valid", errors == 0},
                      {"error_count", errors},
                      {"warning_count", diagnostics.size() - errors},
                      {"diagnostics", shown}};
  if (errors > 0) {
    result["code"] = "invalid_document";
  }
  WritePage(result, page, all.size(), scope);
  return result;
}

LevelJson Apply(CommandTarget& target, const LevelJson& args) {
  const Level& level = RequireLevel(target);
  const LevelEditResult edit = ApplyLevelOperations(level, args["ops"]);
  if (!edit.ok) {
    throw CommandFailure{
        CommandExit::Invalid, "invalid_operations",
        "No changes were made: " + DescribeDiagnostic(edit.errors.front()),
        edit.errors};
  }

  std::vector<std::string> affectedIds;
  const LevelJson report = DescribeLevelChanges(level, edit.level, affectedIds);
  bool truncated = false;
  LevelJson changes = LevelJson::array();
  for (const LevelJson& change : report) {
    if (changes.size() == maximumReportedChanges) {
      truncated = true;
      break;
    }
    LevelJson bounded = change;
    if (bounded.contains("fields")) {
      for (LevelJson& field : bounded["fields"]) {
        if (field.contains("value")) {
          field["value"] = Bounded(field["value"], truncated);
        }
      }
    }
    changes.push_back(bounded);
  }

  const bool dryRun = args.value("dry-run", false);
  LevelJson result = {{"changed", edit.changed && !dryRun},
                      {"would_change", edit.changed},
                      {"dry_run", dryRun},
                      {"affected_ids", affectedIds},
                      {"changes", changes},
                      {"change_count", report.size()},
                      {"changes_truncated", truncated},
                      {"generated_ids", edit.generatedIds},
                      {"completed", nullptr}};
  if (edit.changed && !dryRun) {
    result["completed"] = target.commit(edit.level, "apply");
  }
  return result;
}
}  // namespace

void RegisterDocumentCommands(CommandRegistry& registry) {
  registry.add({
      .name = "commands",
      .description = "List the commands and the targets each supports.",
      .supportsFile = true,
      .supportsEditor = true,
      .resultDescription = "commands: an array of command names, descriptions, "
                           "and supported targets.",
      .run = [&registry](CommandTarget&,
                         const LevelJson&) { return ListCommands(registry); },
  });
  registry.add({
      .name = "commands describe",
      .description =
          "Show one command's argument schema and result descriptions.",
      .supportsFile = true,
      .supportsEditor = true,
      .params = {{"name", "string", true, "The command to describe"}},
      .resultDescription = "describe: argument JSON Schema, side effects, "
                           "positional argument, and result descriptions.",
      .positional = "name",
      .run =
          [&registry](CommandTarget& target, const LevelJson& args) {
            return DescribeCommand(registry, args, target.kind());
          },
  });
  registry.add({
      .name = "inspect",
      .description =
          "Summarise the document, or show one object's fields and transforms.",
      .supportsFile = true,
      .supportsEditor = true,
      .params =
          {{"id", "string", false, "Show this object instead of the summary"},
           {"path", "string", false,
            "With --id: a JSON Pointer into the object's custom properties"}},
      .resultDescription =
          "summary: version, camera, and object counts. With --id: object "
          "fields, local_transform, world_transform (null for shear), "
          "child_count, and truncated. With --path: value and truncated "
          "instead.",
      .run = Inspect,
  });
  std::vector<CommandParam> listParams = {
      {"parent", "string", false, "Only this object's children"},
      {"roots", "boolean", false, "Only root objects"},
      {"subtree", "string", false, "This object and its descendants"},
      {"kind", "string", false,
       "group, marker, box, sphere, cylinder, plane, or camera"},
  };
  const std::vector<CommandParam> pageParams = PageParams();
  listParams.insert(listParams.end(), pageParams.begin(), pageParams.end());
  registry.add({
      .name = "objects list",
      .description = "List object summaries, a page at a time.",
      .supportsFile = true,
      .supportsEditor = true,
      .params = listParams,
      .resultDescription = "objects: paginated ID, name, kind, parent, and "
                           "sibling order in hierarchy order.",
      .run = ListObjects,
  });
  registry.add({
      .name = "validate",
      .description =
          "Audit structure, hierarchy, transforms, and referenced assets "
          "without changing anything.",
      .supportsFile = true,
      .supportsEditor = true,
      .params = pageParams,
      .resultDescription =
          "valid, error_count, warning_count, and paginated diagnostics with "
          "severity, code, message, object ID, and path.",
      .run = Validate,
  });
  registry.add({
      .name = "apply",
      .description =
          "Apply a batch of operations as one transaction. Validates the "
          "result; a live edit is undoable and unsaved, a file edit replaces "
          "the file atomically.",
      .supportsFile = true,
      .supportsEditor = true,
      .mutates = true,
      .hasSideEffects = true,
      .params =
          {{"ops", "json", true,
            "The operation batch: a JSON file path, or - for stdin, on the "
            "command line"},
           {"dry-run", "boolean", false,
            "Report the changes without committing them"}},
      .resultDescription =
          "would_change and dry_run; changes with per-object change kinds, "
          "field paths, and values; change_count before truncation; "
          "changes_truncated; generated_ids by operation index. completed is "
          "live_transaction, file_commit, or null.",
      .run = Apply,
  });
}
