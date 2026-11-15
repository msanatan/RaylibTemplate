#ifndef GBM_PLAYTEST_H
#define GBM_PLAYTEST_H

#include <string>
#include <vector>

#include "core/scene_manager.h"

// Script format: one command per line, `#` starts a comment:
//   press KEY          tap a key for one frame
//   hold KEY FRAMES    keep a key down for that many frames
//   click X Y          left click at a window position
//   wait FRAMES        let the game run
//   shot FILE.png      save a screenshot, relative to the working directory
//   dump               print the scene stack and each scene's debugState()
//   expect TEXT        fail the run unless the dump contains TEXT
//   quit               stop (reaching the end of the script does the same)
// KEY is a letter, a digit, or one of SPACE ENTER ESCAPE TAB BACKSPACE UP DOWN
// LEFT RIGHT SHIFT CONTROL.
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
  struct Command {
    std::string name;
    std::string text;
    int key = 0;
    int x = 0;
    int y = 0;
    int frames = 1;
    int line = 0;
  };

  struct HeldInput {
    int releaseEvent;
    int code;
    int framesLeft;
  };

  void releaseExpiredInputs();

  std::vector<Command> commands;
  std::vector<HeldInput> held;
  size_t next = 0;
  int framesToWait = 0;
  std::string pendingShot;
  bool finished = false;
  bool failed = false;
};

#endif  // !GBM_PLAYTEST_H
