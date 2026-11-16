#include "viewport/gizmo.h"

#include <cmath>

#include "levels/document/transform.h"
#include "raymath.h"
#include "rlgl.h"
#include "viewport/gizmo_library.h"

namespace {
const char* const gestureOwner = "gizmo";

struct ToolBinding {
  ImGuizmo::OPERATION operation;
  const char* field;
  const char* label;
};

ToolBinding Binding(GizmoSettings::Tool tool) {
  switch (tool) {
    case GizmoSettings::Tool::Rotate:
      return {ImGuizmo::ROTATE, "rotation", "Rotate"};
    case GizmoSettings::Tool::Scale:
      return {ImGuizmo::SCALE, "scale", "Scale"};
    default:
      return {ImGuizmo::TRANSLATE, "position", "Move"};
  }
}

float SnapStep(const GizmoSettings& settings) {
  switch (settings.tool) {
    case GizmoSettings::Tool::Rotate:
      return settings.rotateSnap;
    case GizmoSettings::Tool::Scale:
      return settings.scaleSnap;
    default:
      return settings.moveSnap;
  }
}

Matrix Projection(const Camera3D& camera, float aspect) {
  if (camera.projection == CAMERA_ORTHOGRAPHIC) {
    const float halfHeight = camera.fovy / 2.0f;
    return MatrixOrtho(-halfHeight * aspect, halfHeight * aspect, -halfHeight,
                       halfHeight, rlGetCullDistanceNear(),
                       rlGetCullDistanceFar());
  }
  return MatrixPerspective(camera.fovy * DEG2RAD, aspect,
                           rlGetCullDistanceNear(), rlGetCullDistanceFar());
}

// The handles work in single precision; rounding keeps that noise out of
// authored files
double Rounded(double value) { return std::round(value * 1e4) / 1e4; }

LevelVec3 Field(const LevelTransform& transform, GizmoSettings::Tool tool) {
  switch (tool) {
    case GizmoSettings::Tool::Rotate:
      return transform.rotation;
    case GizmoSettings::Tool::Scale:
      return transform.scale;
    default:
      return transform.position;
  }
}
}  // namespace

bool Gizmo::update(EditorSession& session, const Camera3D& camera,
                   Rectangle area) {
  const LevelIndex& index = session.doc().index();
  const LevelObject* object = index.find(session.selection.primary());
  if (object == nullptr || settings.tool == GizmoSettings::Tool::Select) {
    return false;
  }
  const std::string id = object->id;
  const LevelMatrix parent =
      object->parent.has_value() ? index.world(*object->parent) : LevelMatrix();

  float matrix[16];
  const LevelMatrix& world = index.world(id);
  for (size_t i = 0; i < 16; i++) {
    matrix[i] = (float)world.m[i];
  }
  const ToolBinding binding = Binding(settings.tool);
  const float step = SnapStep(settings);
  const float snap[3] = {step, step, step};
  const Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);

  ImGuizmo::SetOrthographic(camera.projection == CAMERA_ORTHOGRAPHIC);
  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(area.x, area.y, area.width, area.height);
  const bool moved = ImGuizmo::Manipulate(
      MatrixToFloatV(view).v,
      MatrixToFloatV(Projection(camera, area.width / area.height)).v,
      binding.operation,
      settings.worldSpace ? ImGuizmo::WORLD : ImGuizmo::LOCAL, matrix, nullptr,
      settings.snap ? snap : nullptr);

  if (!ImGuizmo::IsUsing()) {
    cancelled = false;
    session.finishGesture(gestureOwner, binding.label);
    return ImGuizmo::IsOver();
  }
  if (cancelled) {
    return true;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    cancelled = true;
    session.cancelGesture(gestureOwner);
    return true;
  }

  session.beginGesture(gestureOwner);
  if (!moved) {
    return true;
  }
  LevelMatrix dragged;
  for (size_t i = 0; i < 16; i++) {
    dragged.m[i] = matrix[i];
  }
  // Empty when the drag would need shear or a mirrored scale; the object
  // then stays at the last placement its transform can represent
  const std::optional<LevelTransform> local =
      DecomposeMatrix(Multiply(Inverse(parent), dragged));
  if (local.has_value()) {
    // Only the field the tool edits is written, so a move never disturbs an
    // authored rotation or scale
    const LevelVec3 value = Field(*local, settings.tool);
    session.previewEdit(
        gestureOwner,
        {{"op", "set-transform"},
         {"id", id},
         {binding.field,
          {Rounded(value.x), Rounded(value.y), Rounded(value.z)}}});
  }
  return true;
}
