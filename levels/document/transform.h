#ifndef GBM_LEVEL_TRANSFORM_H
#define GBM_LEVEL_TRANSFORM_H

#include <array>
#include <optional>
#include <string>

#include "levels/document/level.h"

// Column-major, column vectors: world = parentWorld * T * R * S, with
// R = Ry(yaw) * Rx(pitch) * Rz(roll). Double precision keeps authored values
// stable through reparenting.
struct LevelMatrix {
  std::array<double, 16> m = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  double at(int row, int column) const { return m[(size_t)(column * 4 + row)]; }
  double& at(int row, int column) { return m[(size_t)(column * 4 + row)]; }
};

LevelMatrix Multiply(const LevelMatrix& a, const LevelMatrix& b);
// Affine matrices only, which is all a transform hierarchy produces
LevelMatrix Inverse(const LevelMatrix& matrix);
LevelVec3 TransformPoint(const LevelMatrix& matrix, const LevelVec3& point);
LevelVec3 TransformDirection(const LevelMatrix& matrix,
                             const LevelVec3& direction);

LevelMatrix LocalMatrix(const LevelTransform& transform);

// Empty when the matrix needs shear or mirroring, which a position, rotation,
// and positive scale cannot represent
std::optional<LevelTransform> DecomposeMatrix(const LevelMatrix& matrix);

inline constexpr LevelVec3 levelForward = {0.0, 0.0, -1.0};
inline constexpr LevelVec3 levelUp = {0.0, 1.0, 0.0};

#endif  // !GBM_LEVEL_TRANSFORM_H
