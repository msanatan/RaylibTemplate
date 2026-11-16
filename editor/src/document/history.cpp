#include "document/history.h"

void History::record(const Level& before, const std::string& label) {
  undoSteps.push_back({before, label});
  if (undoSteps.size() > maximumSteps) {
    undoSteps.pop_front();
  }
  redoSteps.clear();
}

std::optional<Level> History::undo(const Level& current) {
  if (undoSteps.empty()) {
    return std::nullopt;
  }
  Step step = std::move(undoSteps.back());
  undoSteps.pop_back();
  redoSteps.push_back({current, step.label});
  return std::move(step.level);
}

std::optional<Level> History::redo(const Level& current) {
  if (redoSteps.empty()) {
    return std::nullopt;
  }
  Step step = std::move(redoSteps.back());
  redoSteps.pop_back();
  undoSteps.push_back({current, step.label});
  return std::move(step.level);
}

void History::clear() {
  undoSteps.clear();
  redoSteps.clear();
}

std::string History::undoLabel() const {
  return undoSteps.empty() ? "" : undoSteps.back().label;
}

std::string History::redoLabel() const {
  return redoSteps.empty() ? "" : redoSteps.back().label;
}
