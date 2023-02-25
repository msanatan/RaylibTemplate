#ifndef PLAYER_H
#define PLAYER_H

class Player
{
public:
  float x;
  float y;
  float z;
  float size;

  Player(float x, float y, float z, float size);
  void draw();
};

#endif // !PLAYER_H
