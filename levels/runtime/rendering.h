#ifndef GBM_LEVEL_RENDERING_H
#define GBM_LEVEL_RENDERING_H

#include <map>
#include <string>

#include "levels/document/level.h"
#include "levels/document/level_index.h"
#include "levels/document/transform.h"
#include "raylib.h"

Matrix ToRaylibMatrix(const LevelMatrix& matrix);
Vector3 ToRaylibVector(const LevelVec3& vector);
Color LevelColor(const std::string& hex);

Camera3D LevelCameraView(const LevelIndex& index, const LevelObject& camera);
// raylib keeps clipping distances globally; call before BeginMode3D
void ApplyLevelCameraClipping(const LevelCameraSettings& camera);

class LevelRenderer {
 public:
  explicit LevelRenderer(std::string assetsRoot);
  ~LevelRenderer();
  LevelRenderer(const LevelRenderer&) = delete;
  LevelRenderer& operator=(const LevelRenderer&) = delete;

  void draw(const LevelIndex& index);

  // Loads changed files again. A texture that fails to reload keeps its
  // previous image; one that was missing is retried.
  void reloadTextures();
  int reloadCount() const { return reloads; }

 private:
  struct LoadedTexture {
    Texture2D texture = {};
    bool valid = false;
  };

  const LoadedTexture& texture(const std::string& path);
  LoadedTexture load(const std::string& path) const;

  std::string assetsRoot;
  std::map<std::string, LoadedTexture> textures;
  int reloads = 0;
};

#endif  // !GBM_LEVEL_RENDERING_H
