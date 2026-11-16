#ifndef SCENIC_FILE_WATCH_H
#define SCENIC_FILE_WATCH_H

#include <chrono>
#include <string>

// Notices changed image files under a directory by polling on the editor
// thread, so nothing is ever mutated from a watcher callback. Reports a
// change once writes have settled.
class ImageWatcher {
 public:
  explicit ImageWatcher(std::string directory);
  bool hasChanged();

 private:
  size_t signature() const;

  std::string directory;
  size_t applied = 0;
  size_t lastSeen = 0;
  std::chrono::steady_clock::time_point nextPoll;
};

class Interval {
 public:
  explicit Interval(std::chrono::milliseconds period) : period(period) {}
  bool elapsed();

 private:
  std::chrono::milliseconds period;
  std::chrono::steady_clock::time_point next = std::chrono::steady_clock::now();
};

#endif  // !SCENIC_FILE_WATCH_H
