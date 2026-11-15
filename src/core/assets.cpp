#include "core/assets.h"

#if defined(ASSETS_PAK_PATH)
#include "core/pak.h"
#endif

#if defined(DEV_ASSETS_DIR)
#include <chrono>
#include <filesystem>
#endif

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

#if defined(DEV_ASSETS_DIR)
size_t AssetsSignature() {
  size_t signature = 0;
  std::error_code error;
  for (std::filesystem::recursive_directory_iterator
           file(DEV_ASSETS_DIR, error),
       end;
       !error && file != end; file.increment(error)) {
    const std::string fileName = file->path().filename().string();
    if (!file->is_regular_file(error) || fileName.front() == '.') {
      continue;
    }
    const long long modified =
        (long long)file->last_write_time(error).time_since_epoch().count();
    signature ^= std::hash<std::string>()(file->path().string() +
                                          std::to_string(modified));
  }
  return signature;
}
#endif
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
#if defined(DEV_ASSETS_DIR)
  using Clock = std::chrono::steady_clock;
  static Clock::time_point nextPoll = Clock::now();
  static size_t applied = AssetsSignature();
  static size_t lastSeen = applied;

  if (Clock::now() < nextPoll) {
    return;
  }
  nextPoll = Clock::now() + std::chrono::milliseconds(250);

  // Editors save in several writes, so only act once a whole interval has
  // passed with nothing moving
  const size_t current = AssetsSignature();
  const bool settled = current == lastSeen;
  lastSeen = current;
  if (current == applied || !settled) {
    return;
  }
  applied = current;

  TraceLog(LOG_INFO, "ASSETS: Files changed, reloading");
  // A half-edited shader or data file should show an error and wait for the
  // next save, not end the session
  loadFailuresAreFatal = false;
  onChange();
  loadFailuresAreFatal = true;
#else
  (void)onChange;
#endif
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
