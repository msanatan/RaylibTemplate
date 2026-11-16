#include "viewport/viewport.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "imgui.h"
#include "levels/document/transform.h"
#include "raymath.h"
#include "rlImGui.h"
#include "rlgl.h"
#include "viewport/picking.h"

namespace {
const Color selectionColor = {255, 161, 0, 255};
const Color iconColor = {130, 170, 255, 255};
const int gridLinesPerSide = 30;

void DrawWireBox(Vector3 half, Color color) {
  for (const float y : {-half.y, half.y}) {
    DrawLine3D({-half.x, y, -half.z}, {half.x, y, -half.z}, color);
    DrawLine3D({half.x, y, -half.z}, {half.x, y, half.z}, color);
    DrawLine3D({half.x, y, half.z}, {-half.x, y, half.z}, color);
    DrawLine3D({-half.x, y, half.z}, {-half.x, y, -half.z}, color);
  }
  for (const float x : {-half.x, half.x}) {
    for (const float z : {-half.z, half.z}) {
      DrawLine3D({x, -half.y, z}, {x, half.y, z}, color);
    }
  }
}

void DrawCameraIcon(Color color) {
  const float r = iconRadius;
  const Vector3 corners[4] = {
      {-r, -r * 0.6f, -r * 1.6f},
      {r, -r * 0.6f, -r * 1.6f},
      {r, r * 0.6f, -r * 1.6f},
      {-r, r * 0.6f, -r * 1.6f},
  };
  for (int i = 0; i < 4; i++) {
    DrawLine3D(Vector3Zero(), corners[i], color);
    DrawLine3D(corners[i], corners[(i + 1) % 4], color);
  }
}

void DrawMarkerIcon(Color color) {
  const float r = iconRadius;
  const Vector3 ring[4] = {{r, 0, 0}, {0, 0, r}, {-r, 0, 0}, {0, 0, -r}};
  for (int i = 0; i < 4; i++) {
    DrawLine3D(ring[i], ring[(i + 1) % 4], color);
    DrawLine3D(ring[i], {0, r, 0}, color);
    DrawLine3D(ring[i], {0, -r, 0}, color);
  }
}

void DrawAxes(float length) {
  DrawLine3D({0, 0, 0}, {length, 0, 0}, RED);
  DrawLine3D({0, 0, 0}, {0, length, 0}, GREEN);
  DrawLine3D({0, 0, 0}, {0, 0, length}, BLUE);
}

void DrawIcon(LevelKind kind, Color color) {
  if (kind == LevelKind::Camera) {
    DrawCameraIcon(color);
  } else if (kind == LevelKind::Marker) {
    DrawMarkerIcon(color);
  } else {
    const float r = iconRadius;
    DrawLine3D({-r, 0, 0}, {r, 0, 0}, color);
    DrawLine3D({0, -r, 0}, {0, r, 0}, color);
    DrawLine3D({0, 0, -r}, {0, 0, r}, color);
  }
}

// The grid follows the camera and coarsens as it zooms out, so it frames the
// work at any level size without drawing more lines
void DrawGroundGrid(const EditorCamera& camera, bool dark) {
  const float spacing = camera.distance < 60.0f    ? 1.0f
                        : camera.distance < 600.0f ? 10.0f
                                                   : 100.0f;
  const float block = spacing * 10.0f;
  const float centerX = std::round(camera.target.x / block) * block;
  const float centerZ = std::round(camera.target.z / block) * block;
  const float reach = spacing * gridLinesPerSide;

  const Color minor = dark ? Color{62, 66, 76, 255} : Color{188, 192, 200, 255};
  const Color major =
      dark ? Color{88, 93, 106, 255} : Color{160, 165, 175, 255};
  for (int i = -gridLinesPerSide; i <= gridLinesPerSide; i++) {
    const float x = centerX + (float)i * spacing;
    const float z = centerZ + (float)i * spacing;
    const Color color = i % 10 == 0 ? major : minor;
    DrawLine3D({x, 0, centerZ - reach}, {x, 0, centerZ + reach},
               x == 0.0f ? Color{70, 110, 210, 255} : color);
    DrawLine3D({centerX - reach, 0, z}, {centerX + reach, 0, z},
               z == 0.0f ? Color{200, 70, 70, 255} : color);
  }
}

// Rotation and position only, so icons and axes keep a constant size
Matrix Unscaled(const LevelMatrix& world) {
  Matrix matrix = ToRaylibMatrix(world);
  const Vector3 x = Vector3Normalize({matrix.m0, matrix.m1, matrix.m2});
  const Vector3 y = Vector3Normalize({matrix.m4, matrix.m5, matrix.m6});
  const Vector3 z = Vector3Normalize({matrix.m8, matrix.m9, matrix.m10});
  matrix.m0 = x.x, matrix.m1 = x.y, matrix.m2 = x.z;
  matrix.m4 = y.x, matrix.m5 = y.y, matrix.m6 = y.z;
  matrix.m8 = z.x, matrix.m9 = z.y, matrix.m10 = z.z;
  return matrix;
}

float PixelScale() {
  return IsWindowState(FLAG_WINDOW_HIGHDPI) ? GetWindowScaleDPI().x : 1.0f;
}

Vector3 HalfExtents(const LevelObject& object) {
  return IsPrimitive(object.kind) ? ToRaylibVector(ShapeHalfExtents(object))
                                  : Vector3{iconRadius, iconRadius, iconRadius};
}
}  // namespace

Viewport::Viewport(EditorSession& session)
    : session(session), renderer(session.project.assetsDirectory + "/") {}

Viewport::~Viewport() {
  if (target.id != 0) {
    UnloadRenderTexture(target);
  }
}

const LevelObject* Viewport::previewedCamera() const {
  const Level& level = session.doc().level();
  return previewing && level.camera.has_value()
             ? session.doc().index().find(*level.camera)
             : nullptr;
}

Camera3D Viewport::activeCamera() const {
  const LevelObject* camera = previewedCamera();
  return camera != nullptr ? LevelCameraView(session.doc().index(), *camera)
                           : navigation.view();
}

void Viewport::resizeTarget(int width, int height) {
  if (target.id != 0 && target.texture.width == width &&
      target.texture.height == height) {
    return;
  }
  if (target.id != 0) {
    UnloadRenderTexture(target);
  }
  target = LoadRenderTexture(width, height);
  SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
}

void Viewport::build() {
  previewing = previewedCamera() != nullptr;

  const ImVec2 available = ImGui::GetContentRegionAvail();
  const int width = std::max((int)available.x, 1);
  const int height = std::max((int)available.y, 1);
  resizeTarget((int)((float)width * PixelScale()),
               (int)((float)height * PixelScale()));

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  area = {origin.x, origin.y, (float)width, (float)height};
  // Render textures are stored bottom-up
  rlImGuiImageRect(
      &target.texture, width, height,
      {0, 0, (float)target.texture.width, -(float)target.texture.height});
  const bool hovered = ImGui::IsItemHovered();

  if (previewing) {
    ImGui::SetCursorScreenPos({origin.x + 10, origin.y + 8});
    ImGui::TextColored({1.0f, 0.8f, 0.3f, 1.0f}, "Level camera preview");
  }

  const bool gizmoBusy = session.doc().isOpen() && !previewing &&
                         gizmo.update(session, activeCamera(), area);
  navigate(hovered);
  if (hovered && !gizmoBusy && !navigating &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt) {
    pick();
  }
}

void Viewport::navigate(bool hovered) {
  const ImGuiIO& io = ImGui::GetIO();
  const bool looking = ImGui::IsMouseDown(ImGuiMouseButton_Right);
  const bool orbiting = io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool panning = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
  const bool steering = looking || orbiting || panning;

  if (!navigating) {
    navigating = hovered && steering && !session.doc().isGestureActive();
  } else if (!steering) {
    navigating = false;
  }

  const bool zooming = hovered && io.MouseWheel != 0.0f;
  const bool dragging =
      navigating && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f);
  if (zooming || dragging) {
    previewing = false;
  }
  if (zooming) {
    navigation.zoom(io.MouseWheel);
  }
  const Vector2 delta = {io.MouseDelta.x, io.MouseDelta.y};
  if (dragging && looking) {
    navigation.look(delta);
  } else if (dragging && orbiting) {
    navigation.orbit(delta);
  } else if (dragging) {
    navigation.pan(delta);
  }
  fly();
}

// WASD and QE move the camera whenever the keyboard is not being typed
// into, as in Roblox Studio; Unity only does so while the right mouse
// button is held
void Viewport::fly() {
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput || io.KeyCtrl || io.KeySuper || io.KeyAlt ||
      ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) {
    return;
  }
  const auto axis = [](ImGuiKey positive, ImGuiKey negative) {
    return (ImGui::IsKeyDown(positive) ? 1.0f : 0.0f) -
           (ImGui::IsKeyDown(negative) ? 1.0f : 0.0f);
  };
  const Vector3 direction = {axis(ImGuiKey_D, ImGuiKey_A),
                             axis(ImGuiKey_E, ImGuiKey_Q),
                             axis(ImGuiKey_W, ImGuiKey_S)};
  if (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f) {
    return;
  }
  previewing = false;
  navigation.fly(direction, io.DeltaTime, io.KeyShift);
}

void Viewport::pick() {
  const ImVec2 mouse = ImGui::GetMousePos();
  const Ray ray =
      GetScreenToWorldRayEx({mouse.x - area.x, mouse.y - area.y},
                            activeCamera(), (int)area.width, (int)area.height);
  const std::string id = PickObject(session.doc().index(), ray);
  const ImGuiIO& io = ImGui::GetIO();
  if (io.KeyCtrl || io.KeySuper || io.KeyShift) {
    if (!id.empty()) {
      session.selection.toggle(id);
    }
  } else {
    session.selection.set(id.empty() ? std::vector<std::string>()
                                     : std::vector<std::string>{id});
  }
}

void Viewport::drawOverlays(const LevelIndex& index) const {
  for (const LevelObject& object : index.level().objects) {
    const bool selected = session.selection.contains(object.id);
    if (IsPrimitive(object.kind) && !selected) {
      continue;
    }
    const LevelMatrix& world = index.world(object.id);
    if (IsPrimitive(object.kind)) {
      rlPushMatrix();
      rlMultMatrixf(MatrixToFloat(ToRaylibMatrix(world)));
      DrawWireBox(HalfExtents(object), selectionColor);
      rlPopMatrix();
    }
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(Unscaled(world)));
    if (!IsPrimitive(object.kind)) {
      DrawIcon(object.kind, selected ? selectionColor : iconColor);
    }
    if (selected) {
      DrawAxes(1.0f);
    }
    rlPopMatrix();
  }
}

void Viewport::render() {
  if (target.id == 0) {
    return;
  }
  const LevelIndex& index = session.doc().index();
  if (const LevelObject* camera = previewedCamera()) {
    ApplyLevelCameraClipping(camera->camera);
  } else {
    rlSetClipPlanes(RL_CULL_DISTANCE_NEAR,
                    std::max((double)RL_CULL_DISTANCE_FAR,
                             (double)navigation.distance * 4.0));
  }

  BeginTextureMode(target);
  const bool dark = ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x < 0.5f;
  ClearBackground(dark ? Color{38, 40, 46, 255} : Color{214, 218, 224, 255});
  BeginMode3D(activeCamera());
  if (!previewing) {
    DrawGroundGrid(navigation, dark);
  }
  renderer.draw(index);
  if (!previewing) {
    drawOverlays(index);
  }
  EndMode3D();
  EndTextureMode();
}

bool Viewport::finishScreenshot(std::string& error) {
  const std::string path = screenshotPath;
  screenshotPath.clear();

  std::error_code filesystemError;
  const std::filesystem::path folder =
      std::filesystem::path(path).parent_path();
  if (!folder.empty()) {
    std::filesystem::create_directories(folder, filesystemError);
  }

  Image image;
  if (screenshotViewportOnly) {
    image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
  } else {
    // Drawing is batched until the frame ends, so flush it first
    rlDrawRenderBatchActive();
    image = LoadImageFromScreen();
  }
  const bool saved = IsImageValid(image) && ExportImage(image, path.c_str());
  UnloadImage(image);
  if (!saved) {
    error = "Could not save the screenshot to \"" + path + "\"";
  }
  return saved;
}

void Viewport::frameSelection() { frameObjects(session.selection.ids()); }

Vector2 Viewport::screenPosition(const std::string& id) const {
  const LevelMatrix& world = session.doc().index().world(id);
  const Vector2 inside = GetWorldToScreenEx(
      ToRaylibVector({world.at(0, 3), world.at(1, 3), world.at(2, 3)}),
      activeCamera(), (int)area.width, (int)area.height);
  return {area.x + inside.x, area.y + inside.y};
}

void Viewport::frameObjects(const std::vector<std::string>& ids) {
  previewing = false;
  const LevelIndex& index = session.doc().index();
  std::vector<const LevelObject*> objects;
  for (const std::string& id : ids) {
    if (const LevelObject* object = index.find(id)) {
      objects.push_back(object);
    }
  }
  if (objects.empty()) {
    objects = index.hierarchy();
  }
  if (objects.empty()) {
    navigation.frame(Vector3Zero(), 4.0f);
    return;
  }

  Vector3 low = {FLT_MAX, FLT_MAX, FLT_MAX};
  Vector3 high = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  for (const LevelObject* object : objects) {
    const Vector3 half = HalfExtents(*object);
    for (unsigned int corner = 0; corner < 8; corner++) {
      const Vector3 point = ToRaylibVector(TransformPoint(
          index.world(object->id), {(corner & 1U) != 0 ? half.x : -half.x,
                                    (corner & 2U) != 0 ? half.y : -half.y,
                                    (corner & 4U) != 0 ? half.z : -half.z}));
      low = Vector3Min(low, point);
      high = Vector3Max(high, point);
    }
  }
  navigation.frame(Vector3Scale(Vector3Add(low, high), 0.5f),
                   Vector3Distance(low, high) / 2.0f);
}

bool Viewport::setCameraPreview(bool preview, std::string& error) {
  previewing = preview;
  if (preview && previewedCamera() == nullptr) {
    previewing = false;
    error =
        "The level has no designated camera. Designate one with the "
        "set-level-camera operation, or in a camera's Properties";
    return false;
  }
  return true;
}

void Viewport::requestScreenshot(const std::string& path, bool viewportOnly) {
  screenshotPath = path;
  screenshotViewportOnly = viewportOnly;
}

LevelJson Viewport::viewState() const {
  const Camera3D camera = activeCamera();
  return {
      {"camera", previewing ? "level_camera" : "navigation"},
      {"position", {camera.position.x, camera.position.y, camera.position.z}},
      {"target", {camera.target.x, camera.target.y, camera.target.z}},
      {"texture_reloads", renderer.reloadCount()},
  };
}
