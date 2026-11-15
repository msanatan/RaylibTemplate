#ifndef GBM_PLAYTEST_H
#define GBM_PLAYTEST_H

#include <string>
#include <vector>

#include "core/scenes/scene_manager.h"
#include "core/testing/script.h"

// Executes the commands documented in core/testing/script.h.
class Playtest {
 public:
  // Scripted runs advance by this fixed step, not by wall-clock time, so they
  // always play out the same
  static constexpr float frameTime = 1.0f / 60.0f;

  bool load(const std::string& scriptFile);

  bool isActive() const { return !commands.empty(); }
  bool isFinished() const { return finished; }
  bool hasFailed() const { return failed; }

  void beginFrame(const SceneManager& scenes);
  void endFrame();

 private:
  struct HeldInput {
    int releaseEvent;
    int code;
    int framesLeft;
  };

  void releaseExpiredInputs();

  std::vector<PlaytestCommand> commands;
  std::vector<HeldInput> held;
  size_t next = 0;
  int framesToWait = 0;
  std::string pendingShot;
  bool finished = false;
  bool failed = false;
};

#endif  // !GBM_PLAYTEST_H
