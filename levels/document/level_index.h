#ifndef LEVELS_LEVEL_INDEX_H
#define LEVELS_LEVEL_INDEX_H

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "levels/document/level.h"
#include "levels/document/transform.h"

// Lookups derived from a level's flat object list. An index describes the
// level exactly as it was when built and points into it: build a new one
// after structural changes, and never let it outlive the level. Transform-only
// changes can refresh the affected subtree with updateWorldTransforms().
class LevelIndex {
 public:
  explicit LevelIndex(const Level& level);

  const Level& level() const { return *source; }
  const LevelObject* find(const std::string& id) const;
  const std::vector<const LevelObject*>& children(
      const std::optional<std::string>& parent) const;
  const std::vector<const LevelObject*>& hierarchy() const { return ordered; }
  std::vector<const LevelObject*> subtree(const std::string& id) const;
  bool isDescendant(const std::string& id, const std::string& ancestor) const;

  void updateWorldTransforms(const std::string& id);
  const LevelMatrix& world(const std::string& id) const;
  std::optional<LevelTransform> worldTransform(const std::string& id) const;

 private:
  void visit(const LevelObject& object, const LevelMatrix& parentWorld,
             std::vector<bool>& visited);
  void appendSubtree(const LevelObject& object,
                     std::vector<const LevelObject*>& result) const;

  const Level* source;
  std::unordered_map<std::string, size_t> positions;
  std::map<std::optional<std::string>, std::vector<const LevelObject*>>
      childLists;
  std::vector<const LevelObject*> ordered;
  std::vector<LevelMatrix> worlds;
};

#endif  // !LEVELS_LEVEL_INDEX_H
