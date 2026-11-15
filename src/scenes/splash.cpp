#include "splash.h"

#include <algorithm>

#include "assets.h"

namespace
{
const Color backgroundColor = {11, 15, 31, 255};
const float fadeInDuration = 0.5f;
const float holdDuration = 2.0f;
const float fadeOutDuration = 0.5f;
const float totalDuration = fadeInDuration + holdDuration + fadeOutDuration;
const float logoScreenFraction = 0.5f;
} // namespace

void Splash::load()
{
  logo = LoadGameTexture("branding/logo.png");
  // The logo is drawn far below its source size, and plain bilinear filtering aliases badly at that ratio
  GenTextureMipmaps(&logo);
  SetTextureFilter(logo, TEXTURE_FILTER_TRILINEAR);
  elapsed = 0.0f;
  skipped = false;
}

void Splash::unload()
{
  UnloadTexture(logo);
  logo = {};
}

void Splash::update(float deltaTime)
{
  elapsed += deltaTime;
  if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsGestureDetected(GESTURE_TAP))
  {
    skipped = true;
  }
}

void Splash::draw() const
{
  ClearBackground(backgroundColor);

  const float screenWidth = (float)GetScreenWidth();
  const float screenHeight = (float)GetScreenHeight();
  const float size = std::min(screenWidth, screenHeight) * logoScreenFraction;
  const Rectangle source = {0.0f, 0.0f, (float)logo.width, (float)logo.height};
  const Rectangle dest = {(screenWidth - size) / 2.0f, (screenHeight - size) / 2.0f, size, size};
  DrawTexturePro(logo, source, dest, {0.0f, 0.0f}, 0.0f, Fade(WHITE, logoAlpha()));
}

bool Splash::isFinished() const
{
  return skipped || elapsed >= totalDuration;
}

float Splash::logoAlpha() const
{
  if (elapsed < fadeInDuration)
  {
    return elapsed / fadeInDuration;
  }
  if (elapsed < fadeInDuration + holdDuration)
  {
    return 1.0f;
  }
  return std::max(0.0f, (totalDuration - elapsed) / fadeOutDuration);
}
