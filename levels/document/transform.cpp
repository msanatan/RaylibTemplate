#include "levels/document/transform.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
const double degreesToRadians = std::numbers::pi / 180.0;
const double radiansToDegrees = 180.0 / std::numbers::pi;

LevelMatrix RotationMatrix(const LevelVec3& degrees) {
  const double cx = std::cos(degrees.x * degreesToRadians);
  const double sx = std::sin(degrees.x * degreesToRadians);
  const double cy = std::cos(degrees.y * degreesToRadians);
  const double sy = std::sin(degrees.y * degreesToRadians);
  const double cz = std::cos(degrees.z * degreesToRadians);
  const double sz = std::sin(degrees.z * degreesToRadians);

  LevelMatrix result;
  result.at(0, 0) = cy * cz + sy * sx * sz;
  result.at(0, 1) = -cy * sz + sy * sx * cz;
  result.at(0, 2) = sy * cx;
  result.at(1, 0) = cx * sz;
  result.at(1, 1) = cx * cz;
  result.at(1, 2) = -sx;
  result.at(2, 0) = -sy * cz + cy * sx * sz;
  result.at(2, 1) = sy * sz + cy * sx * cz;
  result.at(2, 2) = cy * cx;
  return result;
}

LevelVec3 EulerFromRotation(const LevelMatrix& rotation) {
  const double sinPitch = -rotation.at(1, 2);
  LevelVec3 degrees;
  if (std::abs(sinPitch) > 0.9999999) {
    // Looking straight up or down: yaw and roll turn about the same axis, so
    // all of it is reported as yaw
    degrees.x = std::copysign(90.0, sinPitch);
    degrees.y =
        std::atan2(-rotation.at(2, 0), rotation.at(0, 0)) * radiansToDegrees;
    degrees.z = 0.0;
  } else {
    degrees.x = std::asin(sinPitch) * radiansToDegrees;
    degrees.y =
        std::atan2(rotation.at(0, 2), rotation.at(2, 2)) * radiansToDegrees;
    degrees.z =
        std::atan2(rotation.at(1, 0), rotation.at(1, 1)) * radiansToDegrees;
  }
  return degrees;
}

// Matrix products leave noise like 1.9999999999999996; authored files should
// keep the value the user meant
double Tidy(double value) {
  const double rounded = std::round(value * 1e9) / 1e9;
  return rounded == 0.0 ? 0.0 : rounded;
}

LevelVec3 Tidy(const LevelVec3& value) {
  return {Tidy(value.x), Tidy(value.y), Tidy(value.z)};
}
}  // namespace

LevelMatrix Multiply(const LevelMatrix& a, const LevelMatrix& b) {
  LevelMatrix result;
  for (int row = 0; row < 4; row++) {
    for (int column = 0; column < 4; column++) {
      double sum = 0.0;
      for (int i = 0; i < 4; i++) {
        sum += a.at(row, i) * b.at(i, column);
      }
      result.at(row, column) = sum;
    }
  }
  return result;
}

LevelMatrix Inverse(const LevelMatrix& matrix) {
  const double a = matrix.at(0, 0), b = matrix.at(0, 1), c = matrix.at(0, 2);
  const double d = matrix.at(1, 0), e = matrix.at(1, 1), f = matrix.at(1, 2);
  const double g = matrix.at(2, 0), h = matrix.at(2, 1), i = matrix.at(2, 2);
  const double determinant =
      a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);

  LevelMatrix result;
  result.at(0, 0) = (e * i - f * h) / determinant;
  result.at(0, 1) = (c * h - b * i) / determinant;
  result.at(0, 2) = (b * f - c * e) / determinant;
  result.at(1, 0) = (f * g - d * i) / determinant;
  result.at(1, 1) = (a * i - c * g) / determinant;
  result.at(1, 2) = (c * d - a * f) / determinant;
  result.at(2, 0) = (d * h - e * g) / determinant;
  result.at(2, 1) = (b * g - a * h) / determinant;
  result.at(2, 2) = (a * e - b * d) / determinant;

  const LevelVec3 translation = {matrix.at(0, 3), matrix.at(1, 3),
                                 matrix.at(2, 3)};
  const LevelVec3 inverted = TransformDirection(result, translation);
  result.at(0, 3) = -inverted.x;
  result.at(1, 3) = -inverted.y;
  result.at(2, 3) = -inverted.z;
  return result;
}

LevelVec3 TransformPoint(const LevelMatrix& matrix, const LevelVec3& point) {
  const LevelVec3 rotated = TransformDirection(matrix, point);
  return {rotated.x + matrix.at(0, 3), rotated.y + matrix.at(1, 3),
          rotated.z + matrix.at(2, 3)};
}

LevelVec3 TransformDirection(const LevelMatrix& matrix,
                             const LevelVec3& direction) {
  return {
      matrix.at(0, 0) * direction.x + matrix.at(0, 1) * direction.y +
          matrix.at(0, 2) * direction.z,
      matrix.at(1, 0) * direction.x + matrix.at(1, 1) * direction.y +
          matrix.at(1, 2) * direction.z,
      matrix.at(2, 0) * direction.x + matrix.at(2, 1) * direction.y +
          matrix.at(2, 2) * direction.z,
  };
}

LevelMatrix LocalMatrix(const LevelTransform& transform) {
  LevelMatrix result = RotationMatrix(transform.rotation);
  const double scale[3] = {transform.scale.x, transform.scale.y,
                           transform.scale.z};
  for (int column = 0; column < 3; column++) {
    for (int row = 0; row < 3; row++) {
      result.at(row, column) *= scale[column];
    }
  }
  result.at(0, 3) = transform.position.x;
  result.at(1, 3) = transform.position.y;
  result.at(2, 3) = transform.position.z;
  return result;
}

std::optional<LevelTransform> DecomposeMatrix(const LevelMatrix& matrix) {
  LevelTransform transform;
  transform.position = {matrix.at(0, 3), matrix.at(1, 3), matrix.at(2, 3)};

  double scale[3];
  LevelMatrix rotation;
  for (int column = 0; column < 3; column++) {
    scale[column] = std::sqrt(matrix.at(0, column) * matrix.at(0, column) +
                              matrix.at(1, column) * matrix.at(1, column) +
                              matrix.at(2, column) * matrix.at(2, column));
    if (!(scale[column] > 0.0) || !std::isfinite(scale[column])) {
      return std::nullopt;
    }
    for (int row = 0; row < 3; row++) {
      rotation.at(row, column) = matrix.at(row, column) / scale[column];
    }
  }

  transform.scale = {scale[0], scale[1], scale[2]};
  transform.rotation = EulerFromRotation(rotation);

  // Shear and mirroring both survive in the difference between the input and
  // what position, rotation, and scale can rebuild
  const LevelMatrix rebuilt = LocalMatrix(transform);
  const double largest = std::max({scale[0], scale[1], scale[2], 1.0});
  for (int column = 0; column < 3; column++) {
    for (int row = 0; row < 3; row++) {
      if (std::abs(rebuilt.at(row, column) - matrix.at(row, column)) >
          1e-6 * largest) {
        return std::nullopt;
      }
    }
  }

  transform.position = Tidy(transform.position);
  transform.rotation = Tidy(transform.rotation);
  transform.scale = Tidy(transform.scale);
  return transform;
}
