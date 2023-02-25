#include "raylib.h"
#include "Player.h"

Player::Player(float x, float y, float z, float size)
{
  this->x = x;
  this->y = y;
  this->z = z;
  this->size = size;
}

void Player::draw()
{
  DrawCube((Vector3){this->x, this->y, this->z}, this->size, this->size, this->size, RED);
  DrawCubeWires((Vector3){this->x, this->y, this->z}, this->size, this->size, this->size, MAROON);
  return;
}
