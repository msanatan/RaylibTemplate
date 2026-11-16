#ifndef SCENIC_SCRIPT_H
#define SCENIC_SCRIPT_H

#include <chrono>
#include <string>
#include <vector>

#include "raylib.h"

class EditorSession;
class Viewport;

// Scripted editor runs for windowed checks, in the spirit of the game's
// playtests: one command per line, `#` starts a comment. A failed command
// ends the run with a failure exit status.
// clang-format off
//   wait FRAMES                     let the editor run
//   sleep MILLISECONDS              keep running for real time, for file watching
//   cmd COMMAND [--argument value]  run a live command; it must succeed.
//                                   $REV and $DOC expand to the document's
//                                   revision and ID
//   cmd_fails CODE COMMAND ...      run a live command; it must fail with CODE
//   click X Y                       left click at a window position
//   click_object ID                 left click an object's origin in the viewport
//   click_target NAME               left click a named widget: row:<object id>
//                                   in the Explorer, field:name or field:position in Properties
//   drag_object ID DX DY FRAMES     drag from an object's origin by a pixel offset
//   drag_target NAME DX DY FRAMES   drag from a named widget by a pixel offset
//   key NAME                        tap a key: a letter, DELETE, BACKSPACE,
//                                   ENTER, or ESCAPE. Prefix SUPER+ or SHIFT+
//   type TEXT                       type into the focused field
//   copy FROM TO                    copy a file, relative to the project
//   shot FILE [viewport|editor]     save a screenshot, relative to the working directory
//   expect TEXT                     fail unless the state dump contains TEXT
//   expect_not TEXT                 fail if the state dump contains TEXT
//   quit                            stop the run successfully
// clang-format on
// @Name anywhere in a command stands for the ID of the object with that name.
// The state dump is the `state` command's JSON on one line without spaces.
class EditorScript {
 public:
  bool load(const std::string& file);
  bool isActive() const { return !commands.empty(); }
  bool isFinished() const { return finished; }
  bool hasFailed() const { return failed; }

  // Runs commands until one takes time. Call before the frame's input is
  // read.
  void beginFrame(EditorSession& session, Viewport& viewport);
  void fail(const std::string& reason);

 private:
  struct Command {
    std::string name;
    std::vector<std::string> words;
    std::string text;
    int line = 0;
  };
  struct HeldInput {
    int releaseEvent;
    int code;
    int framesLeft;
  };

  bool run(EditorSession& session, Viewport& viewport, const Command& command);
  void startDrag(Vector2 from, const std::vector<std::string>& words);
  void click(float x, float y, int frames);
  void releaseExpiredInputs();

  std::vector<Command> commands;
  std::vector<HeldInput> held;
  size_t next = 0;
  int framesToWait = 0;
  int currentLine = 0;
  std::chrono::steady_clock::time_point sleepUntil;
  // A drag in progress: where the pointer is heading and how long it has
  float dragX = 0, dragY = 0, dragStepX = 0, dragStepY = 0;
  int dragFrames = 0;
  bool finished = false;
  bool failed = false;
};

#endif  // !SCENIC_SCRIPT_H
