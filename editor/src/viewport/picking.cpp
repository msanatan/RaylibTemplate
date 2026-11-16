#include "viewport/picking.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "levels/document/transform.h"

namespace {
struct LocalRay {
  LevelVec3 origin;
  LevelVec3 direction;
};

double Component(const LevelVec3& vector, int axis) {
  return axis == 0 ? vector.x : axis == 1 ? vector.y : vector.z;
}

std::optional<double> HitBox(const LocalRay& ray, const LevelVec3& half) {
  double nearest = 0.0;
  double farthest = std::numeric_limits<double>::max();
  for (int axis = 0; axis < 3; axis++) {
    const double origin = Component(ray.origin, axis);
    const double direction = Component(ray.direction, axis);
    const double extent = Component(half, axis);
    if (std::abs(direction) < 1e-12) {
      if (std::abs(origin) > extent) {
        return std::nullopt;
      }
      continue;
    }
    const double first = (-extent - origin) / direction;
    const double second = (extent - origin) / direction;
    nearest = std::max(nearest, std::min(first, second));
    farthest = std::min(farthest, std::max(first, second));
  }
  return nearest <= farthest ? std::optional<double>(nearest) : std::nullopt;
}

std::optional<double> HitSphere(const LocalRay& ray, double radius) {
  const LevelVec3& o = ray.origin;
  const LevelVec3& d = ray.direction;
  const double a = d.x * d.x + d.y * d.y + d.z * d.z;
  const double b = 2.0 * (o.x * d.x + o.y * d.y + o.z * d.z);
  const double c = o.x * o.x + o.y * o.y + o.z * o.z - radius * radius;
  const double discriminant = b * b - 4.0 * a * c;
  if (discriminant < 0.0) {
    return std::nullopt;
  }
  const double root = std::sqrt(discriminant);
  const double nearest = (-b - root) / (2.0 * a);
  const double farthest = (-b + root) / (2.0 * a);
  if (farthest < 0.0) {
    return std::nullopt;
  }
  return std::max(nearest, 0.0);
}

std::optional<double> HitCylinder(const LocalRay& ray, double radius,
                                  double halfHeight) {
  const LevelVec3& o = ray.origin;
  const LevelVec3& d = ray.direction;
  std::optional<double> best;
  const auto consider = [&](double distance) {
    if (distance >= 0.0 && (!best.has_value() || distance < *best)) {
      best = distance;
    }
  };

  const double a = d.x * d.x + d.z * d.z;
  if (a > 1e-12) {
    const double b = 2.0 * (o.x * d.x + o.z * d.z);
    const double c = o.x * o.x + o.z * o.z - radius * radius;
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant >= 0.0) {
      for (const double sign : {-1.0, 1.0}) {
        const double distance =
            (-b + sign * std::sqrt(discriminant)) / (2.0 * a);
        if (std::abs(o.y + distance * d.y) <= halfHeight) {
          consider(distance);
        }
      }
    }
  }
  if (std::abs(d.y) > 1e-12) {
    for (const double cap : {-halfHeight, halfHeight}) {
      const double distance = (cap - o.y) / d.y;
      const double x = o.x + distance * d.x;
      const double z = o.z + distance * d.z;
      if (x * x + z * z <= radius * radius) {
        consider(distance);
      }
    }
  }
  return best;
}
}  // namespace

std::string PickObject(const LevelIndex& index, Ray ray) {
  const LevelVec3 origin = {ray.position.x, ray.position.y, ray.position.z};
  const LevelVec3 direction = {ray.direction.x, ray.direction.y,
                               ray.direction.z};

  std::string nearestId;
  double nearest = std::numeric_limits<double>::max();
  for (const LevelObject& object : index.level().objects) {
    const LevelMatrix& world = index.world(object.id);
    std::optional<double> hit;
    if (IsPrimitive(object.kind)) {
      const LevelMatrix toLocal = Inverse(world);
      const LocalRay local = {TransformPoint(toLocal, origin),
                              TransformDirection(toLocal, direction)};
      LevelVec3 half = ShapeHalfExtents(object);
      if (object.kind == LevelKind::Sphere) {
        hit = HitSphere(local, object.shape.radius);
      } else if (object.kind == LevelKind::Cylinder) {
        hit = HitCylinder(local, object.shape.radius, half.y);
      } else {
        // A plane has no thickness, which would make it impossible to hit
        half.y = std::max(half.y, 0.02);
        hit = HitBox(local, half);
      }
    } else {
      // Icons keep their size whatever the object's scale
      const LocalRay toIcon = {
          {origin.x - world.at(0, 3), origin.y - world.at(1, 3),
           origin.z - world.at(2, 3)},
          direction};
      hit = HitSphere(toIcon, iconRadius);
    }
    if (hit.has_value() && *hit < nearest) {
      nearest = *hit;
      nearestId = object.id;
    }
  }
  return nearestId;
}
