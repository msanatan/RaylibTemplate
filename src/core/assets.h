#ifndef ASSETS_H
#define ASSETS_H

#include <string>

#include "raylib.h"

void InitAssets();
void CloseAssets();

std::string AssetPath(const std::string &relativePath);

Texture2D LoadGameTexture(const std::string &relativePath);
Model LoadGameModel(const std::string &relativePath);
Font LoadGameFont(const std::string &relativePath);
Sound LoadGameSound(const std::string &relativePath);

Music LoadGameMusic(const std::string &relativePath);
void UnloadGameMusic(Music music);

#endif // !ASSETS_H
