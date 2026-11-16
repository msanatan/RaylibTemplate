#include "levels/document/level_index.h"

#include <algorithm>

namespace {
const std::vector<const LevelObject*> noObjects;
const LevelMatrix identity;
}  // namespace

LevelIndex::LevelIndex(const Level& level) : source(&level) {
  const std::vector<LevelObject>& objects = level.objects;
  for (size_t i = 0; i < objects.size(); i++) {
    positions.insert({objects[i].id, i});
    childLists[objects[i].parent].push_back(&objects[i]);
  }
  for (auto& [parent, children] : childLists) {
    std::stable_sort(children.begin(), children.end(),
                     [](const LevelObject* a, const LevelObject* b) {
                       return a->order < b->order;
                     });
  }

  // Objects an invalid level leaves unreachable from the roots (cycles,
  // unknown parents) keep their local transform as their world transform
  worlds.reserve(objects.size());
  for (const LevelObject& object : objects) {
    worlds.push_back(LocalMatrix(object.transform));
  }
  std::vector<bool> visited(objects.size(), false);
  for (const LevelObject* root : children(std::nullopt)) {
    visit(*root, identity, visited);
  }
}

void LevelIndex::visit(const LevelObject& object,
                       const LevelMatrix& parentWorld,
                       std::vector<bool>& visited) {
  const size_t position = (size_t)(&object - source->objects.data());
  if (visited[position]) {
    return;
  }
  visited[position] = true;
  worlds[position] = Multiply(parentWorld, LocalMatrix(object.transform));
  ordered.push_back(&object);
  const LevelMatrix world = worlds[position];
  for (const LevelObject* child : children(object.id)) {
    visit(*child, world, visited);
  }
}

const LevelObject* LevelIndex::find(const std::string& id) const {
  const auto position = positions.find(id);
  return position == positions.end() ? nullptr
                                     : &source->objects[position->second];
}

const std::vector<const LevelObject*>& LevelIndex::children(
    const std::optional<std::string>& parent) const {
  const auto list = childLists.find(parent);
  return list == childLists.end() ? noObjects : list->second;
}

void LevelIndex::appendSubtree(const LevelObject& object,
                               std::vector<const LevelObject*>& result) const {
  // An invalid level can contain a cycle
  if (result.size() > source->objects.size()) {
    return;
  }
  result.push_back(&object);
  for (const LevelObject* child : children(object.id)) {
    appendSubtree(*child, result);
  }
}

std::vector<const LevelObject*> LevelIndex::subtree(
    const std::string& id) const {
  std::vector<const LevelObject*> result;
  if (const LevelObject* object = find(id)) {
    appendSubtree(*object, result);
  }
  return result;
}

bool LevelIndex::isDescendant(const std::string& id,
                              const std::string& ancestor) const {
  const LevelObject* object = find(id);
  for (size_t steps = 0; object != nullptr && object->parent.has_value() &&
                         steps <= source->objects.size();
       steps++) {
    if (*object->parent == ancestor) {
      return true;
    }
    object = find(*object->parent);
  }
  return false;
}

const LevelMatrix& LevelIndex::world(const std::string& id) const {
  const auto position = positions.find(id);
  return position == positions.end() ? identity : worlds[position->second];
}

std::optional<LevelTransform> LevelIndex::worldTransform(
    const std::string& id) const {
  return DecomposeMatrix(world(id));
}

void LevelIndex::updateWorldTransforms(const std::string& id) {
  for (const LevelObject* object : subtree(id)) {
    const size_t position = positions.at(object->id);
    const LevelMatrix parent =
        object->parent.has_value() ? world(*object->parent) : identity;
    worlds[position] = Multiply(parent, LocalMatrix(object->transform));
  }
}
