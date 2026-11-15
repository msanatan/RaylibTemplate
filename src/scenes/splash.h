#ifndef SPLASH_H
#define SPLASH_H

#include "raylib.h"

class Splash
{
public:
  void load();
  void unload();
  void update(float deltaTime);
  void draw() const;
  bool isFinished() const;

private:
  Texture2D logo = {};
  float elapsed = 0.0f;
  bool skipped = false;

  float logoAlpha() const;
};

#endif // !SPLASH_H
