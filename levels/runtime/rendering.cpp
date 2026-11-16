#include "levels/runtime/rendering.h"

#include <cmath>
#include <numbers>

#include "raymath.h"
#include "rlgl.h"

namespace {
const int sphereRings = 16;
const int roundSlices = 32;
// Shown in place of a texture that could not be loaded, so the problem is
// visible without hiding the object
const Color missingTextureColor = MAGENTA;

struct Surface {
  LevelVec2 tiling;
  LevelVec2 offset;

  void vertex(Vector3 position, float u, float v) const {
    rlTexCoord2f(u * (float)tiling.x + (float)offset.x,
                 v * (float)tiling.y + (float)offset.y);
    rlVertex3f(position.x, position.y, position.z);
  }
};

// Counter-clockwise seen from the side `right x up` points to
void Face(const Surface& surface, Vector3 center, Vector3 right, Vector3 up) {
  surface.vertex(Vector3Subtract(Vector3Subtract(center, right), up), 0, 1);
  surface.vertex(Vector3Subtract(Vector3Add(center, right), up), 1, 1);
  surface.vertex(Vector3Add(Vector3Add(center, right), up), 1, 0);
  surface.vertex(Vector3Add(Vector3Subtract(center, right), up), 0, 0);
}

void Box(const Surface& surface, Vector3 half) {
  const Vector3 x = {half.x, 0, 0};
  const Vector3 y = {0, half.y, 0};
  const Vector3 z = {0, 0, half.z};
  Face(surface, z, x, y);
  Face(surface, Vector3Negate(z), Vector3Negate(x), y);
  Face(surface, x, Vector3Negate(z), y);
  Face(surface, Vector3Negate(x), z, y);
  Face(surface, y, x, Vector3Negate(z));
  Face(surface, Vector3Negate(y), x, z);
}

void Plane(const Surface& surface, Vector3 half) {
  const Vector3 x = {half.x, 0, 0};
  const Vector3 z = {0, 0, half.z};
  Face(surface, Vector3Zero(), x, Vector3Negate(z));
  // Visible from below too; a plane has no thickness to cull against
  Face(surface, Vector3Zero(), x, z);
}

void Sphere(const Surface& surface, float radius) {
  const auto point = [&](int ring, int slice) {
    const float polar = (float)std::numbers::pi * (float)ring / sphereRings;
    const float around =
        2.0f * (float)std::numbers::pi * (float)slice / roundSlices;
    surface.vertex(
        {radius * std::sin(polar) * std::cos(around), radius * std::cos(polar),
         radius * std::sin(polar) * std::sin(around)},
        (float)slice / roundSlices, (float)ring / sphereRings);
  };
  for (int ring = 0; ring < sphereRings; ring++) {
    for (int slice = 0; slice < roundSlices; slice++) {
      point(ring, slice);
      point(ring, slice + 1);
      point(ring + 1, slice + 1);
      point(ring + 1, slice);
    }
  }
}

void Cylinder(const Surface& surface, float radius, float height) {
  const float top = height / 2.0f;
  const auto rim = [&](int slice, float y) {
    const float around =
        2.0f * (float)std::numbers::pi * (float)slice / roundSlices;
    return Vector3{radius * std::cos(around), y, radius * std::sin(around)};
  };
  for (int slice = 0; slice < roundSlices; slice++) {
    const float u0 = (float)slice / roundSlices;
    const float u1 = (float)(slice + 1) / roundSlices;
    surface.vertex(rim(slice + 1, -top), u1, 1);
    surface.vertex(rim(slice, -top), u0, 1);
    surface.vertex(rim(slice, top), u0, 0);
    surface.vertex(rim(slice + 1, top), u1, 0);

    // Caps are triangle fans; the batch draws quads, so the last corner
    // repeats
    surface.vertex({0, top, 0}, 0.5f, 0.5f);
    surface.vertex(rim(slice + 1, top), u1, 0);
    surface.vertex(rim(slice, top), u0, 0);
    surface.vertex(rim(slice, top), u0, 0);

    surface.vertex({0, -top, 0}, 0.5f, 0.5f);
    surface.vertex(rim(slice, -top), u0, 1);
    surface.vertex(rim(slice + 1, -top), u1, 1);
    surface.vertex(rim(slice + 1, -top), u1, 1);
  }
}
}  // namespace

Matrix ToRaylibMatrix(const LevelMatrix& matrix) {
  const auto& m = matrix.m;
  return {(float)m[0], (float)m[4], (float)m[8],  (float)m[12],
          (float)m[1], (float)m[5], (float)m[9],  (float)m[13],
          (float)m[2], (float)m[6], (float)m[10], (float)m[14],
          (float)m[3], (float)m[7], (float)m[11], (float)m[15]};
}

Vector3 ToRaylibVector(const LevelVec3& vector) {
  return {(float)vector.x, (float)vector.y, (float)vector.z};
}

Color LevelColor(const std::string& hex) {
  unsigned int value = 0xFFFFFF;
  if (hex.size() == 7 && hex[0] == '#') {
    value = (unsigned int)std::strtoul(hex.c_str() + 1, nullptr, 16);
  }
  return {(unsigned char)(value >> 16), (unsigned char)(value >> 8),
          (unsigned char)value, 255};
}

Camera3D LevelCameraView(const LevelIndex& index, const LevelObject& camera) {
  const LevelMatrix& world = index.world(camera.id);
  Camera3D view = {};
  view.position =
      ToRaylibVector({world.at(0, 3), world.at(1, 3), world.at(2, 3)});
  view.target =
      Vector3Add(view.position, Vector3Normalize(ToRaylibVector(
                                    TransformDirection(world, levelForward))));
  view.up =
      Vector3Normalize(ToRaylibVector(TransformDirection(world, levelUp)));
  view.projection =
      camera.camera.orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
  view.fovy = (float)(camera.camera.orthographic ? camera.camera.size
                                                 : camera.camera.fov);
  return view;
}

void ApplyLevelCameraClipping(const LevelCameraSettings& camera) {
  rlSetClipPlanes(camera.nearPlane, camera.farPlane);
}

LevelRenderer::LevelRenderer(std::string assetsRoot)
    : assetsRoot(std::move(assetsRoot)) {}

LevelRenderer::~LevelRenderer() {
  for (const auto& [path, loaded] : textures) {
    if (loaded.valid) {
      UnloadTexture(loaded.texture);
    }
  }
}

LevelRenderer::LoadedTexture LevelRenderer::load(
    const std::string& path) const {
  LoadedTexture loaded;
  loaded.texture = LoadTexture((assetsRoot + path).c_str());
  loaded.valid = IsTextureValid(loaded.texture);
  if (loaded.valid) {
    SetTextureWrap(loaded.texture, TEXTURE_WRAP_REPEAT);
  }
  return loaded;
}

const LevelRenderer::LoadedTexture& LevelRenderer::texture(
    const std::string& path) {
  const auto existing = textures.find(path);
  if (existing != textures.end()) {
    return existing->second;
  }
  const LoadedTexture loaded = load(path);
  if (!loaded.valid) {
    TraceLog(LOG_WARNING,
             "LEVEL: Texture [%s] could not be loaded; drawing a placeholder",
             path.c_str());
  }
  return textures[path] = loaded;
}

void LevelRenderer::reloadTextures() {
  for (auto& [path, current] : textures) {
    const LoadedTexture replacement = load(path);
    if (!replacement.valid) {
      TraceLog(LOG_WARNING, "LEVEL: Texture [%s] failed to reload%s",
               path.c_str(),
               current.valid ? "; keeping the previous image" : "");
      continue;
    }
    if (current.valid) {
      UnloadTexture(current.texture);
    }
    current = replacement;
  }
  reloads++;
}

void LevelRenderer::draw(const LevelIndex& index) {
  for (const LevelObject& object : index.level().objects) {
    if (!IsPrimitive(object.kind)) {
      continue;
    }

    Color color = LevelColor(object.appearance.color);
    unsigned int textureId = rlGetTextureIdDefault();
    if (object.appearance.texture.has_value()) {
      const LoadedTexture& loaded = texture(*object.appearance.texture);
      if (loaded.valid) {
        textureId = loaded.texture.id;
      } else {
        color = missingTextureColor;
      }
    }

    const Surface surface = {object.appearance.tiling,
                             object.appearance.offset};
    const Vector3 half = ToRaylibVector(ShapeHalfExtents(object));
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(ToRaylibMatrix(index.world(object.id))));
    rlSetTexture(textureId);
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    if (object.kind == LevelKind::Box) {
      Box(surface, half);
    } else if (object.kind == LevelKind::Plane) {
      Plane(surface, half);
    } else if (object.kind == LevelKind::Sphere) {
      Sphere(surface, (float)object.shape.radius);
    } else {
      Cylinder(surface, (float)object.shape.radius, (float)object.shape.height);
    }
    rlEnd();
    rlSetTexture(0);
    rlPopMatrix();
  }
}
