#include "app/file_watch.h"

#include <filesystem>
#include <functional>

namespace {
const auto pollPeriod = std::chrono::milliseconds(300);
}

ImageWatcher::ImageWatcher(std::string directory)
    : directory(std::move(directory)),
      nextPoll(std::chrono::steady_clock::now()) {
  applied = signature();
  lastSeen = applied;
}

size_t ImageWatcher::signature() const {
  size_t result = 0;
  std::error_code error;
  for (std::filesystem::recursive_directory_iterator file(directory, error),
       end;
       !error && file != end; file.increment(error)) {
    const std::string extension = file->path().extension().string();
    const bool image = extension == ".png" || extension == ".jpg" ||
                       extension == ".jpeg" || extension == ".bmp" ||
                       extension == ".tga";
    if (!image || !file->is_regular_file(error)) {
      continue;
    }
    const long long modified =
        (long long)file->last_write_time(error).time_since_epoch().count();
    result ^= std::hash<std::string>()(file->path().string() +
                                       std::to_string(modified) +
                                       std::to_string(file->file_size(error)));
  }
  return result;
}

bool ImageWatcher::hasChanged() {
  if (std::chrono::steady_clock::now() < nextPoll) {
    return false;
  }
  nextPoll = std::chrono::steady_clock::now() + pollPeriod;

  // Image editors save in several writes; wait for a whole interval with
  // nothing moving before reloading
  const size_t current = signature();
  const bool settled = current == lastSeen;
  lastSeen = current;
  if (current == applied || !settled) {
    return false;
  }
  applied = current;
  return true;
}

bool Interval::elapsed() {
  const auto now = std::chrono::steady_clock::now();
  if (now < next) {
    return false;
  }
  next = now + period;
  return true;
}
