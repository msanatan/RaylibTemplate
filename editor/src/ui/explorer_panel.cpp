#include "ui/explorer_panel.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "document/session.h"
#include "imgui.h"
#include "ui/actions.h"
#include "ui/script_targets.h"
#include "viewport/viewport.h"

namespace {
const char* const dragPayload = "SCENIC_OBJECT";

const LevelKind creatableKinds[] = {
    LevelKind::Group,    LevelKind::Marker, LevelKind::Box,   LevelKind::Sphere,
    LevelKind::Cylinder, LevelKind::Plane,  LevelKind::Camera};

// Rows are drawn from IDs collected up front because drawing a row can edit
// the level, which replaces every object
std::vector<std::string> ChildIds(const EditorSession& session,
                                  const std::optional<std::string>& parent) {
  std::vector<std::string> ids;
  for (const LevelObject* child : session.doc().index().children(parent)) {
    ids.push_back(child->id);
  }
  return ids;
}

void SelectFromClick(EditorSession& session, const std::string& id) {
  const ImGuiIO& io = ImGui::GetIO();
  if (io.KeyCtrl || io.KeySuper || io.KeyShift) {
    session.selection.toggle(id);
  } else {
    session.selection.set({id});
  }
}

void AcceptReparentDrop(EditorSession& session,
                        const std::optional<std::string>& parent) {
  if (!ImGui::BeginDragDropTarget()) {
    return;
  }
  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(dragPayload)) {
    ReparentObject(session, (const char*)payload->Data, parent);
  }
  ImGui::EndDragDropTarget();
}
}  // namespace

void ExplorerPanel::drawCreateItems(const UiContext& ui,
                                    const std::optional<std::string>& parent) {
  for (const LevelKind kind : creatableKinds) {
    std::string label = LevelKindName(kind);
    label[0] = (char)std::toupper((unsigned char)label[0]);
    if (ImGui::MenuItem(label.c_str())) {
      CreateObject(ui.session, kind,
                   parent.has_value() ? LevelVec3() : ui.viewport.focusPoint(),
                   parent);
    }
  }
}

void ExplorerPanel::drawRowMenu(const UiContext& ui, const std::string& id) {
  EditorSession& session = ui.session;
  if (!session.selection.contains(id)) {
    session.selection.set({id});
  }
  if (ImGui::BeginMenu("Create Child")) {
    drawCreateItems(ui, id);
    ImGui::EndMenu();
  }
  if (ImGui::MenuItem("Duplicate")) {
    DuplicateSelection(session);
  }
  if (ImGui::MenuItem("Delete")) {
    DeleteSelection(session);
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Move Up")) {
    MoveSibling(session, id, -1);
  }
  if (ImGui::MenuItem("Move Down")) {
    MoveSibling(session, id, 1);
  }
  if (ImGui::MenuItem("Move to Root")) {
    ReparentObject(session, id, std::nullopt);
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Frame in Viewport")) {
    ui.viewport.frameObjects({id});
  }
}

void ExplorerPanel::drawRow(const UiContext& ui, const std::string& id) {
  EditorSession& session = ui.session;
  const LevelIndex& index = session.doc().index();
  const LevelObject* object = index.find(id);
  if (object == nullptr) {
    return;
  }

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                             ImGuiTreeNodeFlags_SpanAvailWidth |
                             ImGuiTreeNodeFlags_DefaultOpen;
  if (index.children(id).empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf;
  }
  if (session.selection.contains(id)) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
  const std::string name = object->name.empty() ? "(unnamed)" : object->name;
  const std::string label =
      name + "  [" + LevelKindName(object->kind) +
      (session.doc().level().camera == id ? ", level camera" : "") + "]";
  const bool open = ImGui::TreeNodeEx(id.c_str(), flags, "%s", label.c_str());
  RecordScriptTarget("row:" + id);

  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
    SelectFromClick(session, id);
  }
  if (ImGui::IsItemHovered() &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    ui.viewport.frameObjects({id});
  }
  if (ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload(dragPayload, id.c_str(), id.size() + 1);
    ImGui::TextUnformatted(name.c_str());
    ImGui::EndDragDropSource();
  }
  AcceptReparentDrop(session, id);
  if (ImGui::BeginPopupContextItem()) {
    drawRowMenu(ui, id);
    ImGui::EndPopup();
  }

  if (open) {
    for (const std::string& child : ChildIds(session, id)) {
      drawRow(ui, child);
    }
    ImGui::TreePop();
  }
}

void ExplorerPanel::draw(const UiContext& ui) {
  EditorSession& session = ui.session;
  if (!session.doc().isOpen()) {
    ImGui::TextDisabled("No level is open");
    return;
  }

  if (ImGui::Button("+ Create")) {
    ImGui::OpenPopup("create");
  }
  if (ImGui::BeginPopup("create")) {
    drawCreateItems(ui, std::nullopt);
    ImGui::EndPopup();
  }
  const std::string primary = session.selection.primary();
  ImGui::BeginDisabled(primary.empty());
  ImGui::SameLine();
  if (ImGui::Button("Up")) {
    MoveSibling(session, primary, -1);
  }
  ImGui::SameLine();
  if (ImGui::Button("Down")) {
    MoveSibling(session, primary, 1);
  }
  ImGui::EndDisabled();
  ImGui::Separator();

  ImGui::BeginChild("tree");
  for (const std::string& root : ChildIds(session, std::nullopt)) {
    drawRow(ui, root);
  }
  const ImVec2 remaining = ImGui::GetContentRegionAvail();
  ImGui::InvisibleButton(
      "root-drop", {std::max(remaining.x, 1.0f), std::max(remaining.y, 40.0f)});
  if (ImGui::IsItemClicked()) {
    session.selection.clear();
  }
  AcceptReparentDrop(session, std::nullopt);
  ImGui::EndChild();
}
