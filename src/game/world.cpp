#include "game/world.h"

#include <sstream>

#include "levels/document/transform.h"
#include "levels/runtime/rendering.h"
#include "raymath.h"

World::World(const Level& authored, const Camera3D& fallbackCamera)
    : current(authored), index(current), view(fallbackCamera) {
  for (const LevelObject& object : current.objects) {
    if (object.properties.value("role", LevelJson()) == "player") {
      playerId = object.id;
      const LevelMatrix& world = index.world(object.id);
      playerState.position =
          ToRaylibVector({world.at(0, 3), world.at(1, 3), world.at(2, 3)});
      playerState.size =
          (float)(object.shape.size.x * object.transform.scale.x);
      break;
    }
  }

  const LevelObject* camera =
      current.camera.has_value() ? current.find(*current.camera) : nullptr;
  if (camera != nullptr) {
    view = LevelCameraView(index, *camera);
    cameraLens = camera->camera;
  }
}

void World::movePlayer(Vector3 velocity) {
  placePlayer(Vector3Add(playerState.position, velocity));
}

void World::placePlayer(Vector3 position) {
  if (position.x == playerState.position.x &&
      position.y == playerState.position.y &&
      position.z == playerState.position.z) {
    return;
  }
  playerState.position = position;
  const LevelObject* indexed = index.find(playerId);
  LevelObject* object =
      indexed == nullptr ? nullptr
                         : &current.objects[indexed - current.objects.data()];
  if (object == nullptr) {
    return;
  }
  const LevelMatrix parent =
      object->parent.has_value() ? index.world(*object->parent) : LevelMatrix();
  object->transform.position =
      TransformPoint(Inverse(parent), {position.x, position.y, position.z});
  index.updateWorldTransforms(playerId);
}

void World::controlCamera(const Camera3D& camera) {
  view = camera;
  cameraGameControlled = true;
}

std::string World::saveState() const {
  std::string state =
      TextFormat("%f %f %f", playerState.position.x, playerState.position.y,
                 playerState.position.z);
  if (cameraGameControlled) {
    state += TextFormat(" camera %f %f %f %f %f %f", view.position.x,
                        view.position.y, view.position.z, view.target.x,
                        view.target.y, view.target.z);
  }
  return state;
}

void World::restoreState(const std::string& state) {
  std::istringstream stream(state);
  Vector3 position = playerState.position;
  if (stream >> position.x >> position.y >> position.z) {
    placePlayer(position);
  }

  std::string marker;
  Camera3D camera = view;
  if (stream >> marker >> camera.position.x >> camera.position.y >>
          camera.position.z >> camera.target.x >> camera.target.y >>
          camera.target.z &&
      marker == "camera") {
    controlCamera(camera);
  }
}
