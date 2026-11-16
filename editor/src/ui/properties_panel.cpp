#include "ui/properties_panel.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "document/session.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "levels/runtime/rendering.h"
#include "ui/script_targets.h"

namespace {
const ImVec4 errorColor = {1.0f, 0.45f, 0.45f, 1.0f};
const char* const colorGesture = "properties:color";

bool Section(const char* title) {
  return ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);
}

void Label(const char* text) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(text);
  ImGui::SameLine(std::max(120.0f, ImGui::GetContentRegionAvail().x * 0.38f));
  ImGui::SetNextItemWidth(-FLT_MIN);
}

std::string Hidden(const char* label) { return std::string("##") + label; }

std::string PropertyPath(std::string key) {
  for (size_t i = 0; i < key.size(); i++) {
    if (key[i] == '~' || key[i] == '/') {
      key.replace(i, 1, key[i] == '~' ? "~0" : "~1");
      i++;
    }
  }
  return "/" + key;
}

// Sliders work in single precision; rounding keeps 0.1 from being stored as
// 0.10000000149
LevelJson Rounded(const float* values, int count) {
  LevelJson array = LevelJson::array();
  for (int i = 0; i < count; i++) {
    array.push_back(std::round((double)values[i] * 1e5) / 1e5);
  }
  return count == 1 ? array[0] : array;
}

void ReadOnlyVector(const char* label, const LevelVec3& value,
                    const char* format) {
  float values[3] = {(float)value.x, (float)value.y, (float)value.z};
  Label(label);
  ImGui::BeginDisabled();
  ImGui::DragFloat3((Hidden(label) + "world").c_str(), values, 0, 0, 0, format);
  ImGui::EndDisabled();
}
}  // namespace

void PropertiesPanel::dragNumbers(EditorSession& session, const std::string& id,
                                  const NumberField& field,
                                  std::initializer_list<double> values) {
  float edited[3] = {0, 0, 0};
  const int count = std::min((int)values.size(), 3);
  std::transform(values.begin(), values.begin() + count, edited,
                 [](double value) { return (float)value; });

  Label(field.label);
  const std::string widget = Hidden(field.label);
  const bool changed =
      count == 3 ? ImGui::DragFloat3(widget.c_str(), edited, field.speed, 0, 0,
                                     field.format)
      : count == 2 ? ImGui::DragFloat2(widget.c_str(), edited, field.speed, 0,
                                       0, field.format)
                   : ImGui::DragFloat(widget.c_str(), edited, field.speed, 0, 0,
                                      field.format);
  RecordScriptTarget(std::string("field:") + field.field);

  const std::string owner = std::string("properties:") + field.label;
  if (changed) {
    session.beginGesture(owner);
    session.previewEdit(owner, {{"op", field.operation},
                                {"id", id},
                                {field.field, Rounded(edited, count)}});
  }
  if (ImGui::IsItemDeactivated()) {
    session.finishGesture(owner, field.label);
  }
}

void PropertiesPanel::drawIdentity(EditorSession& session,
                                   const LevelObject& object) {
  if (!Section("Identity")) {
    return;
  }
  std::string name = object.name;
  Label("Name");
  ImGui::InputText("##name", &name);
  RecordScriptTarget("field:name");
  // Committed when focus leaves the field, so typing is one undo step
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    session.edit({{"op", "rename"}, {"id", object.id}, {"name", name}},
                 "Rename");
  }
  Label("Kind");
  ImGui::TextUnformatted(LevelKindName(object.kind));
  Label("ID");
  ImGui::TextUnformatted(object.id.c_str());
  ImGui::SameLine();
  if (ImGui::SmallButton("Copy")) {
    ImGui::SetClipboardText(object.id.c_str());
  }
}

void PropertiesPanel::drawTransform(
    EditorSession& session, const LevelObject& object,
    const std::optional<LevelTransform>& world) {
  if (!Section("Transform")) {
    return;
  }
  const LevelTransform& local = object.transform;

  ImGui::SeparatorText("Local");
  dragNumbers(session, object.id,
              {"Position (m)", "set-transform", "position", 0.05f, "%.3f"},
              {local.position.x, local.position.y, local.position.z});
  dragNumbers(session, object.id,
              {"Rotation (deg)", "set-transform", "rotation", 0.5f, "%.2f"},
              {local.rotation.x, local.rotation.y, local.rotation.z});
  dragNumbers(session, object.id,
              {"Scale (x)", "set-transform", "scale", 0.01f, "%.3f"},
              {local.scale.x, local.scale.y, local.scale.z});

  ImGui::SeparatorText("World");
  if (!world.has_value()) {
    ImGui::TextWrapped(
        "A rotated, non-uniformly scaled parent shears this object, so its "
        "world rotation and scale cannot be shown.");
    return;
  }
  ReadOnlyVector("Position (m)", world->position, "%.3f");
  ReadOnlyVector("Rotation (deg)", world->rotation, "%.2f");
  ReadOnlyVector("Scale (x)", world->scale, "%.3f");
}

void PropertiesPanel::drawShape(EditorSession& session,
                                const LevelObject& object) {
  if (!Section("Shape")) {
    return;
  }
  const LevelShape& shape = object.shape;
  if (object.kind == LevelKind::Box) {
    dragNumbers(session, object.id,
                {"Size (m)", "set-shape", "size", 0.05f, "%.3f"},
                {shape.size.x, shape.size.y, shape.size.z});
    return;
  }
  if (object.kind == LevelKind::Plane) {
    dragNumbers(session, object.id,
                {"Size X, Z (m)", "set-shape", "size", 0.05f, "%.3f"},
                {shape.size.x, shape.size.z});
    return;
  }
  dragNumbers(session, object.id,
              {"Radius (m)", "set-shape", "radius", 0.05f, "%.3f"},
              {shape.radius});
  if (object.kind == LevelKind::Cylinder) {
    dragNumbers(session, object.id,
                {"Height (m)", "set-shape", "height", 0.05f, "%.3f"},
                {shape.height});
  }
}

void PropertiesPanel::drawCamera(EditorSession& session,
                                 const LevelObject& object) {
  if (!Section("Camera")) {
    return;
  }
  const LevelCameraSettings& camera = object.camera;

  bool isLevelCamera = session.doc().level().camera == object.id;
  Label("Level camera");
  if (ImGui::Checkbox("##level-camera", &isLevelCamera)) {
    session.edit(
        {{"op", "set-level-camera"},
         {"camera", isLevelCamera ? LevelJson(object.id) : LevelJson()}},
        "Set Level Camera");
  }
  ImGui::SetItemTooltip("The camera that supplies the game's initial view");

  int projection = camera.orthographic ? 1 : 0;
  Label("Projection");
  if (ImGui::Combo("##projection", &projection,
                   "Perspective\0Orthographic\0")) {
    session.edit(
        {{"op", "set-camera"},
         {"id", object.id},
         {"projection", projection == 1 ? "orthographic" : "perspective"}},
        "Set Projection");
  }
  if (camera.orthographic) {
    dragNumbers(session, object.id,
                {"Height (m)", "set-camera", "size", 0.1f, "%.2f"},
                {camera.size});
  } else {
    dragNumbers(session, object.id,
                {"Field of view (deg)", "set-camera", "fov", 0.25f, "%.1f"},
                {camera.fov});
  }
  dragNumbers(session, object.id,
              {"Near (m)", "set-camera", "near", 0.01f, "%.3f"},
              {camera.nearPlane});
  dragNumbers(session, object.id,
              {"Far (m)", "set-camera", "far", 1.0f, "%.1f"},
              {camera.farPlane});
}

void PropertiesPanel::drawColor(EditorSession& session,
                                const LevelObject& object) {
  const Color color = LevelColor(object.appearance.color);
  float rgb[3] = {(float)color.r / 255.0f, (float)color.g / 255.0f,
                  (float)color.b / 255.0f};
  Label("Base color");
  const bool changed =
      ImGui::ColorEdit3("##color", rgb, ImGuiColorEditFlags_DisplayRGB);
  if (changed) {
    pickingColor = true;
    session.beginGesture(colorGesture);
    session.previewEdit(
        colorGesture,
        {{"op", "set-appearance"},
         {"id", object.id},
         {"color", TextFormat("#%02X%02X%02X", (int)std::lround(rgb[0] * 255),
                              (int)std::lround(rgb[1] * 255),
                              (int)std::lround(rgb[2] * 255))}});
  }
  // The picker popup's sliders are separate widgets, so the gesture ends
  // once nothing in it is held any more
  if (pickingColor && !changed && !ImGui::IsAnyItemActive()) {
    pickingColor = false;
    session.finishGesture(colorGesture, "Set Color");
  }

  std::string hex = object.appearance.color;
  Label("Hex");
  ImGui::InputText("##hex", &hex);
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    session.edit({{"op", "set-appearance"}, {"id", object.id}, {"color", hex}},
                 "Set Color");
  }
}

void PropertiesPanel::drawTexture(EditorSession& session,
                                  const LevelObject& object) {
  const LevelAppearance& appearance = object.appearance;
  std::string texture = appearance.texture.value_or("");
  Label("Texture");
  ImGui::InputTextWithHint("##texture", "none (drop an image here)", &texture);
  bool edited = ImGui::IsItemDeactivatedAfterEdit();
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(assetDragPayload)) {
      texture = (const char*)payload->Data;
      edited = true;
    }
    ImGui::EndDragDropTarget();
  }
  if (appearance.texture.has_value()) {
    Label("");
    if (ImGui::SmallButton("Clear texture")) {
      texture.clear();
      edited = true;
    }
  }
  if (edited) {
    session.edit(
        {{"op", "set-appearance"},
         {"id", object.id},
         {"texture", texture.empty() ? LevelJson() : LevelJson(texture)}},
        "Set Texture");
  }
  if (!appearance.texture.has_value()) {
    return;
  }

  if (!std::filesystem::exists(
          std::filesystem::path(session.project.assetsDirectory) /
          *appearance.texture)) {
    ImGui::TextColored(errorColor, "Missing file; a placeholder is drawn");
  }
  dragNumbers(session, object.id,
              {"Tiling", "set-appearance", "tiling", 0.05f, "%.3f"},
              {appearance.tiling.x, appearance.tiling.y});
  dragNumbers(session, object.id,
              {"Offset", "set-appearance", "offset", 0.01f, "%.3f"},
              {appearance.offset.x, appearance.offset.y});
}

void PropertiesPanel::drawAppearance(EditorSession& session,
                                     const LevelObject& object) {
  if (!Section("Appearance")) {
    return;
  }
  drawColor(session, object);
  drawTexture(session, object);
}

void PropertiesPanel::applyGameData(EditorSession& session,
                                    const LevelObject& object) {
  const LevelJson edited = LevelJson::parse(draftText, nullptr, false);
  if (!edited.is_object()) {
    draftError =
        "Properties must be a JSON object, such as {\"role\": \"player\"}";
    return;
  }
  // Sent as property operations so only what changed is touched
  LevelJson operations = LevelJson::array();
  for (const auto& [key, value] : object.properties.items()) {
    if (!edited.contains(key)) {
      operations.push_back({{"op", "remove-property"},
                            {"id", object.id},
                            {"path", PropertyPath(key)}});
    }
  }
  for (const auto& [key, value] : edited.items()) {
    if (!object.properties.contains(key) || object.properties[key] != value) {
      operations.push_back({{"op", "set-property"},
                            {"id", object.id},
                            {"path", PropertyPath(key)},
                            {"value", value}});
    }
  }
  draftError =
      session.edit(operations, "Edit Properties") ? "" : session.lastOperation;
  draftRevision.clear();
}

void PropertiesPanel::drawGameData(EditorSession& session,
                                   const LevelObject& object) {
  if (!Section("Game Data")) {
    return;
  }
  const std::string authored = object.properties.dump(2);
  const bool stale =
      draftObject != object.id || draftRevision != session.doc().revision();
  if (stale && !ImGui::IsAnyItemActive()) {
    draftObject = object.id;
    draftRevision = session.doc().revision();
    draftText = authored;
    draftError.clear();
  }

  ImGui::InputTextMultiline("##properties", &draftText,
                            {-FLT_MIN, ImGui::GetTextLineHeight() * 9});
  ImGui::BeginDisabled(draftText == authored);
  if (ImGui::Button("Apply")) {
    applyGameData(session, object);
  }
  ImGui::SameLine();
  if (ImGui::Button("Revert")) {
    draftRevision.clear();
  }
  ImGui::EndDisabled();
  if (!draftError.empty()) {
    ImGui::TextColored(errorColor, "%s", draftError.c_str());
  }
}

void PropertiesPanel::draw(const UiContext& ui) {
  EditorSession& session = ui.session;
  const LevelIndex& index = session.doc().index();
  const LevelObject* selected = index.find(session.selection.primary());
  if (selected == nullptr) {
    ImGui::TextDisabled(session.doc().isOpen() ? "Select an object to edit it"
                                               : "No level is open");
    return;
  }
  if (session.selection.ids().size() > 1) {
    ImGui::TextDisabled("%d objects selected; showing the last one",
                        (int)session.selection.ids().size());
  }

  // Copies, because an edit in one section replaces the level, and with it
  // every object and the index, while later sections are still being drawn
  const LevelObject object = *selected;
  const std::optional<LevelTransform> world = index.worldTransform(object.id);
  drawIdentity(session, object);
  drawTransform(session, object, world);
  if (IsPrimitive(object.kind)) {
    drawShape(session, object);
    drawAppearance(session, object);
  }
  if (object.kind == LevelKind::Camera) {
    drawCamera(session, object);
  }
  drawGameData(session, object);
}
