#ifndef GBM_ASSETS_H
#define GBM_ASSETS_H

#include <functional>
#include <string>

#include "raylib.h"

void InitAssets();
void CloseAssets();

// Only Debug desktop builds watch for changes; everywhere else this does nothing
void PollAssetChanges(const std::function<void()> &onChange);

std::string AssetPath(const std::string &relativePath);

Texture2D LoadGameTexture(const std::string &relativePath);
Model LoadGameModel(const std::string &relativePath);
Font LoadGameFont(const std::string &relativePath);
Sound LoadGameSound(const std::string &relativePath);

Music LoadGameMusic(const std::string &relativePath);
void UnloadGameMusic(Music music);

#endif // !GBM_ASSETS_H
