#include "ui/actions.h"

#include <algorithm>
#include <cctype>

#include "app/process.h"
#include "raylib.h"

namespace {
std::vector<std::string> SelectedRoots(const EditorSession& session) {
  const std::vector<std::string>& selected = session.selection.ids();
  const LevelIndex& index = session.doc().index();
  std::vector<std::string> roots;
  for (const std::string& id : selected) {
    const bool covered = std::any_of(selected.begin(), selected.end(),
                                     [&](const std::string& other) {
                                       return index.isDescendant(id, other);
                                     });
    if (!covered) {
      roots.push_back(id);
    }
  }
  return roots;
}
}  // namespace

std::string CreateObject(EditorSession& session, LevelKind kind,
                         const LevelVec3& position,
                         const std::optional<std::string>& parent) {
  std::string name = LevelKindName(kind);
  name[0] = (char)std::toupper((unsigned char)name[0]);

  // Primitives are centred on their origin, so lifting one by half its
  // height rests it on the ground
  LevelObject shape;
  shape.kind = kind;
  const double lift = IsPrimitive(kind) ? ShapeHalfExtents(shape).y : 0.0;

  std::string id = NewLevelId();
  LevelJson create = {
      {"op", "create"},
      {"id", id},
      {"name", name},
      {"kind", LevelKindName(kind)},
      {"transform",
       {{"position", {position.x, position.y + lift, position.z}}}},
  };
  if (parent.has_value()) {
    create["parent"] = *parent;
  }
  if (!session.edit(create, "Create " + name)) {
    return "";
  }
  session.selection.set({id});
  return id;
}

void DuplicateSelection(EditorSession& session) {
  LevelJson operations = LevelJson::array();
  std::vector<std::string> copies;
  for (const std::string& id : SelectedRoots(session)) {
    LevelJson newIds = LevelJson::object();
    for (const LevelObject* object : session.doc().index().subtree(id)) {
      newIds[object->id] = NewLevelId();
    }
    copies.push_back(newIds[id].get<std::string>());
    operations.push_back({{"op", "duplicate"}, {"id", id}, {"ids", newIds}});
  }
  if (!operations.empty() && session.edit(operations, "Duplicate")) {
    session.selection.set(copies);
  }
}

void DeleteSelection(EditorSession& session) {
  LevelJson operations = LevelJson::array();
  for (const std::string& id : SelectedRoots(session)) {
    operations.push_back({{"op", "delete"}, {"id", id}, {"subtree", true}});
  }
  if (!operations.empty()) {
    session.edit(operations, "Delete");
  }
}

void ReparentObject(EditorSession& session, const std::string& id,
                    const std::optional<std::string>& parent) {
  const LevelObject* object = session.doc().index().find(id);
  if (object == nullptr || object->parent == parent) {
    return;
  }
  session.edit(
      {{"op", "reparent"},
       {"id", id},
       {"parent", parent.has_value() ? LevelJson(*parent) : LevelJson()},
       {"mode", "keep-world"}},
      "Reparent");
}

void MoveSibling(EditorSession& session, const std::string& id, int delta) {
  const LevelIndex& index = session.doc().index();
  const LevelObject* object = index.find(id);
  if (object == nullptr) {
    return;
  }
  const int siblings = (int)index.children(object->parent).size();
  const int position = object->order + delta;
  if (position >= 0 && position < siblings) {
    session.edit({{"op", "reorder"}, {"id", id}, {"index", position}},
                 "Reorder");
  }
}

void PlayLevel(EditorSession& session, bool fresh) {
  if (session.doc().isDirty() && !session.trySave()) {
    return;
  }
  std::string error;
  const Project& project = session.project;
  if (LaunchProcess(project.gameExecutable,
                    fresh ? project.resetArguments : project.playArguments,
                    project.root, error)) {
    TraceLog(LOG_INFO, "SCENIC: Launched the game%s",
             fresh ? " from the authored level" : "");
  } else {
    TraceLog(LOG_ERROR, "SCENIC: %s", error.c_str());
  }
}
