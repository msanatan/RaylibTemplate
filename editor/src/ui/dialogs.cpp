#include "ui/dialogs.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include "document/session.h"
#include "imgui.h"
#include "imgui_stdlib.h"

namespace {
std::vector<std::string> LevelFiles(const Project& project) {
  std::vector<std::string> files;
  std::error_code error;
  for (std::filesystem::recursive_directory_iterator
           file(project.assetsDirectory, error),
       end;
       !error && file != end; file.increment(error)) {
    if (file->path().extension() == ".json") {
      files.push_back(
          file->path().lexically_relative(project.root).generic_string());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}
}  // namespace

void LevelDialogs::promptNewLevel() {
  pathPrompt = Action::New;
  pathInput = "assets/levels/untitled.json";
}

void LevelDialogs::promptOpenLevel() {
  pathPrompt = Action::Open;
  pathInput.clear();
}

void LevelDialogs::requestOpen(EditorSession& session,
                               const std::string& path) {
  request(session, Action::Open, path);
}

void LevelDialogs::requestNew(EditorSession& session, const std::string& path) {
  request(session, Action::New, path);
}

void LevelDialogs::requestQuit(EditorSession& session) {
  request(session, Action::Quit, "");
}

void LevelDialogs::request(EditorSession& session, Action action,
                           const std::string& path) {
  pending = action;
  pendingPath = path.empty() ? "" : session.project.resolve(path);
  if (!session.doc().isDirty()) {
    perform(session);
  }
}

void LevelDialogs::perform(EditorSession& session) {
  const Action action = pending;
  pending = Action::None;
  if (action == Action::Quit) {
    quit = true;
  } else if (action == Action::Open) {
    session.tryOpenLevel(pendingPath);
  } else if (action == Action::New) {
    session.tryNewLevel(pendingPath);
  }
}

void LevelDialogs::draw(EditorSession& session) {
  drawPathPrompt(session);
  drawUnsavedPrompt(session);
  drawDiskConflictPrompt(session);
}

void LevelDialogs::drawPathPrompt(EditorSession& session) {
  if (pathPrompt != Action::None) {
    ImGui::OpenPopup("Level Path");
  }
  if (!ImGui::BeginPopupModal("Level Path", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  const bool creating = pathPrompt == Action::New;
  ImGui::TextUnformatted(creating ? "Path for the new level, in the project:"
                                  : "Level to open, in the project:");
  ImGui::SetNextItemWidth(420);
  ImGui::InputText("##path", &pathInput);
  if (!creating) {
    for (const std::string& file : LevelFiles(session.project)) {
      if (ImGui::Selectable(file.c_str(), file == pathInput,
                            ImGuiSelectableFlags_NoAutoClosePopups)) {
        pathInput = file;
      }
    }
  }

  ImGui::BeginDisabled(pathInput.empty());
  const bool confirmed = ImGui::Button(creating ? "Create" : "Open");
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (confirmed || ImGui::Button("Cancel")) {
    const Action action = pathPrompt;
    pathPrompt = Action::None;
    ImGui::CloseCurrentPopup();
    if (confirmed) {
      request(session, action, pathInput);
    }
  }
  ImGui::EndPopup();
}

void LevelDialogs::drawUnsavedPrompt(EditorSession& session) {
  if (pending != Action::None && pathPrompt == Action::None) {
    ImGui::OpenPopup("Unsaved Changes");
  }
  if (!ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  ImGui::TextUnformatted("This level has unsaved changes.");
  if (ImGui::Button("Save")) {
    if (session.trySave()) {
      perform(session);
    } else {
      pending = Action::None;
    }
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Discard")) {
    perform(session);
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    pending = Action::None;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

void LevelDialogs::drawDiskConflictPrompt(EditorSession& session) {
  if (session.doc().hasDiskConflict() && pending == Action::None) {
    ImGui::OpenPopup("Changed on Disk");
  }
  if (!ImGui::BeginPopupModal("Changed on Disk", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  ImGui::TextUnformatted(
      "The level file changed outside Scenic, and you have unsaved edits.");
  if (ImGui::Button("Reload from Disk")) {
    session.reloadFromDisk();
    ImGui::CloseCurrentPopup();
  }
  ImGui::SetItemTooltip("Discards your unsaved edits");
  ImGui::SameLine();
  if (ImGui::Button("Keep My Edits")) {
    session.keepLocalChanges();
    ImGui::CloseCurrentPopup();
  }
  ImGui::SetItemTooltip("Your next save replaces the file on disk");
  ImGui::EndPopup();
}
