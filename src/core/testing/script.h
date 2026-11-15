#ifndef GBM_PLAYTEST_SCRIPT_H
#define GBM_PLAYTEST_SCRIPT_H

#include <string>
#include <vector>

// Script format: one command per line, `#` starts a comment.
// clang-format off
//   press KEY          tap a key for one frame
//   hold KEY FRAMES    keep a key down for that many frames
//   click X Y          left click at a window position
//   touch X Y FRAMES   hold a finger with on-screen controls enabled
//   wait FRAMES        let the game run
//   shot FILE.png      save a screenshot relative to the working directory
//   dump               print the scene stack and each scene's debugState()
//   expect TEXT        fail unless the dump contains TEXT
//   quit               stop the test successfully
//   expect_exit        require the preceding command to have exited the game
// clang-format on
// Reaching the end also stops the test successfully. quit and expect_exit must
// be the last command. An empty script or an unexpected game exit is an error.
// KEY is a letter, a digit, or one of SPACE ENTER ESCAPE TAB BACKSPACE UP DOWN
// LEFT RIGHT SHIFT CONTROL.
struct PlaytestCommand {
  std::string name;
  std::string text;
  int key = 0;
  int x = 0;
  int y = 0;
  int frames = 1;
  int line = 0;
};

// Replaces commands on success; leaves them unchanged on failure.
// Logs the filename and line on invalid input.
bool LoadPlaytestScript(const std::string& scriptFile,
                        std::vector<PlaytestCommand>& commands);

#endif  // !GBM_PLAYTEST_SCRIPT_H
