#ifndef GBM_PLAYER_H
#define GBM_PLAYER_H

#include "raylib.h"

class Player {
 public:
  Vector3 position;
  float size;

  Player(float x, float y, float z, float size);
  void draw() const;
  void move(Vector3 velocity);
};

#endif  // !GBM_PLAYER_H
