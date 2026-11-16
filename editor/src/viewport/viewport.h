#ifndef SCENIC_VIEWPORT_H
#define SCENIC_VIEWPORT_H

#include <string>
#include <vector>

#include "document/session.h"
#include "levels/runtime/rendering.h"
#include "raylib.h"
#include "viewport/camera.h"
#include "viewport/gizmo.h"

class Viewport : public EditorView {
 public:
  explicit Viewport(EditorSession& session);
  ~Viewport() override;
  Viewport(const Viewport&) = delete;
  Viewport& operator=(const Viewport&) = delete;

  void build();
  // Call after every panel has been built, so this frame's edits are drawn
  void render();

  bool hasPendingScreenshot() const { return !screenshotPath.empty(); }
  bool finishScreenshot(std::string& error);

  void frameSelection();
  LevelVec3 focusPoint() const {
    return {navigation.target.x, 0.0, navigation.target.z};
  }
  Vector2 screenPosition(const std::string& id) const;
  void reloadTextures() { renderer.reloadTextures(); }

  void frameObjects(const std::vector<std::string>& ids) override;
  bool setCameraPreview(bool preview, std::string& error) override;
  bool isPreviewingCamera() const override { return previewing; }
  void requestScreenshot(const std::string& path, bool viewportOnly) override;
  LevelJson viewState() const override;

  Gizmo gizmo;

 private:
  const LevelObject* previewedCamera() const;
  Camera3D activeCamera() const;
  void resizeTarget(int width, int height);
  void navigate(bool hovered);
  void fly();
  void pick();
  void drawOverlays(const LevelIndex& index) const;

  EditorSession& session;
  LevelRenderer renderer;
  EditorCamera navigation;
  bool previewing = false;
  RenderTexture2D target = {};
  Rectangle area = {0, 0, 1, 1};
  bool navigating = false;
  std::string screenshotPath;
  bool screenshotViewportOnly = true;
};

#endif  // !SCENIC_VIEWPORT_H
