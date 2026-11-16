#include "document/selection.h"

#include <algorithm>

bool Selection::contains(const std::string& id) const {
  return std::find(selected.begin(), selected.end(), id) != selected.end();
}

std::string Selection::primary() const {
  return selected.empty() ? "" : selected.back();
}

void Selection::set(std::vector<std::string> ids) { selected = std::move(ids); }

void Selection::toggle(const std::string& id) {
  if (std::erase(selected, id) == 0) {
    selected.push_back(id);
  }
}

void Selection::dropMissing(const LevelIndex& index) {
  std::erase_if(selected, [&](const std::string& id) {
    return index.find(id) == nullptr;
  });
}
