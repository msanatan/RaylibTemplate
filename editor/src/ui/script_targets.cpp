#include "ui/script_targets.h"

#include "imgui.h"

std::map<std::string, Rectangle>& ScriptTargets() {
  static std::map<std::string, Rectangle> targets;
  return targets;
}

void RecordScriptTarget(const std::string& name) {
  const ImVec2 low = ImGui::GetItemRectMin();
  const ImVec2 high = ImGui::GetItemRectMax();
  ScriptTargets()[name] = {low.x, low.y, high.x - low.x, high.y - low.y};
}
