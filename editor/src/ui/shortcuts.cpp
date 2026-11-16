#include "ui/shortcuts.h"

#include "document/session.h"
#include "imgui.h"
#include "ui/actions.h"
#include "viewport/viewport.h"

namespace {
bool Chord(ImGuiKey key, bool shift = false) {
  const ImGuiIO& io = ImGui::GetIO();
  return (io.KeySuper || io.KeyCtrl) && io.KeyShift == shift &&
         ImGui::IsKeyPressed(key, false);
}

void HandleChords(EditorSession& session) {
  if (Chord(ImGuiKey_S)) {
    session.trySave();
  } else if (Chord(ImGuiKey_Z, true) || Chord(ImGuiKey_Y)) {
    session.redo();
  } else if (Chord(ImGuiKey_Z)) {
    session.undo();
  } else if (Chord(ImGuiKey_D)) {
    DuplicateSelection(session);
  }
}

void HandleToolKeys(const UiContext& ui) {
  using Tool = GizmoSettings::Tool;
  GizmoSettings& gizmo = ui.viewport.gizmo.settings;
  if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) ||
      ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
    DeleteSelection(ui.session);
  } else if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
    ui.viewport.frameSelection();
  } else if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
    gizmo.tool = Tool::Select;
  } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
    gizmo.tool = Tool::Move;
  } else if (ImGui::IsKeyPressed(ImGuiKey_3, false)) {
    gizmo.tool = Tool::Rotate;
  } else if (ImGui::IsKeyPressed(ImGuiKey_4, false)) {
    gizmo.tool = Tool::Scale;
  }
}
}  // namespace

void HandleShortcuts(const UiContext& ui) {
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput ||
      ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) {
    return;
  }
  HandleChords(ui.session);
  const bool modified = io.KeySuper || io.KeyCtrl || io.KeyAlt;
  if (!modified) {
    HandleToolKeys(ui);
  }
}
