#ifndef SCENIC_HISTORY_H
#define SCENIC_HISTORY_H

#include <deque>
#include <optional>
#include <string>

#include "levels/document/level.h"

// Undo and redo as whole-level snapshots, which is simple and safe while
// levels are small. The oldest steps are dropped beyond `maximumSteps`, so
// memory stays bounded.
class History {
 public:
  static constexpr size_t maximumSteps = 100;

  void record(const Level& before, const std::string& label);
  std::optional<Level> undo(const Level& current);
  std::optional<Level> redo(const Level& current);
  void clear();

  size_t undoCount() const { return undoSteps.size(); }
  size_t redoCount() const { return redoSteps.size(); }
  std::string undoLabel() const;
  std::string redoLabel() const;

 private:
  struct Step {
    Level level;
    std::string label;
  };

  std::deque<Step> undoSteps;
  std::deque<Step> redoSteps;
};

#endif  // !SCENIC_HISTORY_H
