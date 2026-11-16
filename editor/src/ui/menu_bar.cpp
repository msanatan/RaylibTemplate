#include "ui/menu_bar.h"

#include <filesystem>

#include "document/session.h"
#include "imgui.h"
#include "raylib.h"
#include "ui/actions.h"
#include "ui/dialogs.h"
#include "ui/layout.h"
#include "viewport/viewport.h"

namespace {
void DrawFileMenu(const UiContext& ui) {
  if (ImGui::MenuItem("New Level...")) {
    ui.dialogs.promptNewLevel();
  }
  if (ImGui::MenuItem("Open Level...")) {
    ui.dialogs.promptOpenLevel();
  }
  if (ImGui::MenuItem("Save", "Cmd/Ctrl+S", false, ui.session.doc().isOpen())) {
    ui.session.trySave();
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Quit")) {
    ui.dialogs.requestQuit(ui.session);
  }
}

void DrawEditMenu(EditorSession& session) {
  const History& history = session.doc().history();
  const bool hasSelection = !session.selection.ids().empty();
  if (ImGui::MenuItem(("Undo " + history.undoLabel()).c_str(), "Cmd/Ctrl+Z",
                      false, history.undoCount() > 0)) {
    session.undo();
  }
  if (ImGui::MenuItem(("Redo " + history.redoLabel()).c_str(),
                      "Shift+Cmd/Ctrl+Z", false, history.redoCount() > 0)) {
    session.redo();
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Duplicate", "Cmd/Ctrl+D", false, hasSelection)) {
    DuplicateSelection(session);
  }
  if (ImGui::MenuItem("Delete", "Del", false, hasSelection)) {
    DeleteSelection(session);
  }
}

void DrawViewMenu(const UiContext& ui, Preferences& preferences,
                  bool& resetLayout) {
  const bool open = ui.session.doc().isOpen();
  if (ImGui::MenuItem("Frame Selected", "F", false, open)) {
    ui.viewport.frameSelection();
  }
  bool previewing = ui.viewport.isPreviewingCamera();
  if (ImGui::MenuItem("Preview Level Camera", nullptr, &previewing, open)) {
    std::string error;
    if (!ui.viewport.setCameraPreview(previewing, error)) {
      TraceLog(LOG_WARNING, "SCENIC: %s", error.c_str());
    }
  }
  ImGui::Separator();
  for (const bool dark : {true, false}) {
    if (ImGui::MenuItem(dark ? "Dark Theme" : "Light Theme", nullptr,
                        preferences.darkTheme == dark)) {
      preferences.darkTheme = dark;
      ApplyTheme(dark);
      SavePreferences(ui.session.project.preferencesDirectory, preferences);
    }
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Reset Layout")) {
    resetLayout = true;
  }
}

void DrawDocumentTitle(const EditorSession& session) {
  const std::string title = std::filesystem::path(session.doc().path())
                                .lexically_relative(session.project.root)
                                .generic_string() +
                            (session.doc().isDirty() ? "  (unsaved)" : "");
  ImGui::SameLine(ImGui::GetWindowWidth() -
                  ImGui::CalcTextSize(title.c_str()).x - 16.0f);
  ImGui::TextDisabled("%s", title.c_str());
}
}  // namespace

void DrawMenuBar(const UiContext& ui, Preferences& preferences,
                 bool& resetLayout) {
  if (!ImGui::BeginMainMenuBar()) {
    return;
  }
  if (ImGui::BeginMenu("File")) {
    DrawFileMenu(ui);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Edit")) {
    DrawEditMenu(ui.session);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("View")) {
    DrawViewMenu(ui, preferences, resetLayout);
    ImGui::EndMenu();
  }
  if (ui.session.doc().isOpen()) {
    DrawDocumentTitle(ui.session);
  }
  ImGui::EndMainMenuBar();
}
