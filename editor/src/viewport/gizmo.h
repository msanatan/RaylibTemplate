#ifndef SCENIC_GIZMO_H
#define SCENIC_GIZMO_H

#include "document/session.h"
#include "raylib.h"

struct GizmoSettings {
  enum class Tool { Select, Move, Rotate, Scale };
  Tool tool = Tool::Move;
  bool worldSpace = true;
  bool snap = false;
  float moveSnap = 0.5f;
  float rotateSnap = 15.0f;
  float scaleSnap = 0.1f;
};

class Gizmo {
 public:
  // Draws into `area` of the current ImGui window. Returns true while the
  // handles own the mouse, so the viewport does not also pick.
  bool update(EditorSession& session, const Camera3D& camera, Rectangle area);

  GizmoSettings settings;

 private:
  // Escape abandons a drag, but the handles stay grabbed until the mouse is
  // released; movement in between must be ignored
  bool cancelled = false;
};

#endif  // !SCENIC_GIZMO_H
