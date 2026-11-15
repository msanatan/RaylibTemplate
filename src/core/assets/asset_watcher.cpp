#include "core/assets/asset_watcher.h"

#if defined(DEV_ASSETS_DIR)
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#endif

namespace {
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

bool HaveAssetsChanged() {
#if defined(DEV_ASSETS_DIR)
  using Clock = std::chrono::steady_clock;
  static Clock::time_point nextPoll = Clock::now();
  static size_t applied = AssetsSignature();
  static size_t lastSeen = applied;

  if (Clock::now() < nextPoll) {
    return false;
  }
  nextPoll = Clock::now() + std::chrono::milliseconds(250);

  // Editors save in several writes, so only act once a whole interval has
  // passed with nothing moving
  const size_t current = AssetsSignature();
  const bool settled = current == lastSeen;
  lastSeen = current;
  if (current == applied || !settled) {
    return false;
  }
  applied = current;

  return true;
#else
  return false;
#endif
}
