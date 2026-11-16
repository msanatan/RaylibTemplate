#include "ui/toolbar.h"

#include "document/session.h"
#include "extras/IconsFontAwesome6.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "raylib.h"
#include "ui/actions.h"
#include "viewport/viewport.h"

namespace {
using Tool = GizmoSettings::Tool;

bool ToolButton(const char* label, bool active, const char* tooltip) {
  if (active) {
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  }
  const bool clicked = ImGui::Button(label);
  if (active) {
    ImGui::PopStyleColor();
  }
  ImGui::SetItemTooltip("%s", tooltip);
  ImGui::SameLine();
  return clicked;
}

void Divider() {
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();
}

void DrawTools(GizmoSettings& gizmo) {
  struct ToolChoice {
    Tool tool;
    const char* label;
    const char* tooltip;
  };
  const ToolChoice choices[] = {
      {Tool::Select, "Select", "Select only (1)"},
      {Tool::Move, "Move", "Move (2)"},
      {Tool::Rotate, "Rotate", "Rotate (3)"},
      {Tool::Scale, "Scale", "Scale (4)"},
  };
  for (const ToolChoice& choice : choices) {
    if (ToolButton(choice.label, gizmo.tool == choice.tool, choice.tooltip)) {
      gizmo.tool = choice.tool;
    }
  }
}

void DrawSnapping(GizmoSettings& gizmo) {
  if (ToolButton(gizmo.worldSpace ? "World" : "Local", false,
                 "Handles follow the world axes or the object's own")) {
    gizmo.worldSpace = !gizmo.worldSpace;
  }
  ImGui::Checkbox("Snap", &gizmo.snap);
  ImGui::SetItemTooltip("Snap to %.2f m, %.0f degrees, or %.2fx",
                        gizmo.moveSnap, gizmo.rotateSnap, gizmo.scaleSnap);
  ImGui::SameLine();
  if (!gizmo.snap) {
    return;
  }
  float* step = gizmo.tool == Tool::Rotate  ? &gizmo.rotateSnap
                : gizmo.tool == Tool::Scale ? &gizmo.scaleSnap
                                            : &gizmo.moveSnap;
  ImGui::SetNextItemWidth(70);
  ImGui::DragFloat("##step", step, 0.05f, 0.01f, 90.0f, "%.2f");
  ImGui::SameLine();
}

void DrawViewControls(Viewport& viewport) {
  if (ToolButton("Frame", false, "Frame the selection (F)")) {
    viewport.frameSelection();
  }
  if (ToolButton("Camera", viewport.isPreviewingCamera(),
                 "Preview the level camera")) {
    std::string error;
    if (!viewport.setCameraPreview(!viewport.isPreviewingCamera(), error)) {
      TraceLog(LOG_WARNING, "SCENIC: %s", error.c_str());
    }
  }
}
}  // namespace

void DrawToolbar(const UiContext& ui) {
  DrawTools(ui.viewport.gizmo.settings);
  Divider();
  DrawSnapping(ui.viewport.gizmo.settings);
  Divider();
  DrawViewControls(ui.viewport);
  Divider();
  if (ToolButton(ICON_FA_PLAY " Play", false,
                 "Save and run the game, keeping gameplay progress")) {
    PlayLevel(ui.session, false);
  }
  if (ToolButton(ICON_FA_ROTATE_LEFT " Reset", false,
                 "Save and run the game from the authored level")) {
    PlayLevel(ui.session, true);
  }
  ImGui::NewLine();
}
