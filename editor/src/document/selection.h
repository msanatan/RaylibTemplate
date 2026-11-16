#ifndef SCENIC_SELECTION_H
#define SCENIC_SELECTION_H

#include <string>
#include <vector>

#include "levels/document/level_index.h"

class Selection {
 public:
  const std::vector<std::string>& ids() const { return selected; }
  bool contains(const std::string& id) const;
  std::string primary() const;

  void set(std::vector<std::string> ids);
  void toggle(const std::string& id);
  void clear() { selected.clear(); }
  void dropMissing(const LevelIndex& index);

 private:
  std::vector<std::string> selected;
};

#endif  // !SCENIC_SELECTION_H
