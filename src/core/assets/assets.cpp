#include "core/assets/assets.h"

#if defined(ASSETS_PAK_PATH)
#include "core/assets/pak.h"
#endif

#include "core/assets/asset_watcher.h"

namespace {
const std::string& AssetsRoot() {
  static const std::string root =
#if defined(PLATFORM_WEB)
      "/assets/";
#elif defined(DEV_ASSETS_DIR)
      // Debug desktop builds read the source folder directly, so asset edits
      // show up without a rebuild
      DEV_ASSETS_DIR;
#else
      // Not a real directory: pak.cpp serves it. The leading slash stops raylib
      // prefixing "./" to a model's sibling files
      "/assets.pak/";
#endif
  return root;
}

#if defined(NDEBUG)
bool loadFailuresAreFatal = false;
#else
bool loadFailuresAreFatal = true;
#endif

void RequireLoaded(bool loaded, const char* kind,
                   const std::string& relativePath) {
  if (!loaded) {
    TraceLog(loadFailuresAreFatal ? LOG_FATAL : LOG_ERROR,
             "ASSETS: Failed to load %s [%s]", kind, relativePath.c_str());
  }
}

}  // namespace

void InitAssets() {
#if defined(ASSETS_PAK_PATH)
  MountPak(std::string(GetApplicationDirectory()) + ASSETS_PAK_PATH,
           AssetsRoot());
#endif
}

void CloseAssets() {
#if defined(ASSETS_PAK_PATH)
  UnmountPak();
#endif
}

void PollAssetChanges(const std::function<void()>& onChange) {
  if (!HaveAssetsChanged()) {
    return;
  }

  TraceLog(LOG_INFO, "ASSETS: Files changed, reloading");
  // A half-edited shader or data file should show an error and wait for the
  // next save, not end the session
  loadFailuresAreFatal = false;
  onChange();
  loadFailuresAreFatal = true;
}

std::string AssetPath(const std::string& relativePath) {
  return AssetsRoot() + relativePath;
}

Texture2D LoadGameTexture(const std::string& relativePath) {
  const Texture2D texture = LoadTexture(AssetPath(relativePath).c_str());
  RequireLoaded(IsTextureValid(texture), "texture", relativePath);
  return texture;
}

Model LoadGameModel(const std::string& relativePath) {
  const Model model = LoadModel(AssetPath(relativePath).c_str());
  RequireLoaded(IsModelValid(model), "model", relativePath);
  return model;
}

Font LoadGameFont(const std::string& relativePath) {
  const Font font = LoadFont(AssetPath(relativePath).c_str());
  // raylib returns its built-in font when loading fails
  RequireLoaded(font.texture.id != GetFontDefault().texture.id, "font",
                relativePath);
  return font;
}

Sound LoadGameSound(const std::string& relativePath) {
  const Sound sound = LoadSound(AssetPath(relativePath).c_str());
  RequireLoaded(IsSoundValid(sound), "sound", relativePath);
  return sound;
}

Music LoadGameMusic(const std::string& relativePath) {
#if defined(ASSETS_PAK_PATH)
  const Music music = LoadPakMusic(relativePath);
#else
  const Music music = LoadMusicStream(AssetPath(relativePath).c_str());
#endif
  RequireLoaded(IsMusicValid(music), "music", relativePath);
  return music;
}

void UnloadGameMusic(Music music) {
#if defined(ASSETS_PAK_PATH)
  UnloadPakMusic(music);
#else
  UnloadMusicStream(music);
#endif
}
