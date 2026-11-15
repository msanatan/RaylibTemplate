#ifndef GBM_PLAYTEST_SCRIPT_H
#define GBM_PLAYTEST_SCRIPT_H

#include <string>
#include <vector>

// Script format: one command per line, `#` starts a comment:
//   press KEY          tap a key for one frame
//   hold KEY FRAMES    keep a key down for that many frames
//   click X Y          left click at a window position
//   touch X Y FRAMES   hold a finger on the screen (turns on-screen controls
//   on) wait FRAMES        let the game run shot FILE.png      save a
//   screenshot, relative to the working directory dump               print the
//   scene stack and each scene's debugState() expect TEXT        fail the run
//   unless the dump contains TEXT quit               stop (reaching the end of
//   the script does the same)
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

// Appends parsed commands, logging the filename and line on invalid input.
bool LoadPlaytestScript(const std::string& scriptFile,
                        std::vector<PlaytestCommand>& commands);

#endif  // !GBM_PLAYTEST_SCRIPT_H
