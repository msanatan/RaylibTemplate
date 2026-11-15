#include "core/testing/playtest.h"

#include <filesystem>
#include <sstream>

#include "core/input/input.h"
#include "raylib.h"
#include "rlgl.h"

namespace {
// raylib's AutomationEventType values, which rcore.c keeps private
const int eventKeyUp = 1;
const int eventKeyDown = 2;
const int eventMouseButtonUp = 5;
const int eventMouseButtonDown = 6;
const int eventMousePosition = 7;

void Inject(int event, int first, int second = 0) {
  PlayAutomationEvent({0, (unsigned int)event, {first, second, 0, 0}});
}

void LogLines(int level, const std::string& lines) {
  std::istringstream stream(lines);
  std::string line;
  while (std::getline(stream, line)) {
    TraceLog(level, "PLAYTEST:   %s", line.c_str());
  }
}

}  // namespace

bool Playtest::load(const std::string& scriptFile) {
  return LoadPlaytestScript(scriptFile, commands);
}

void Playtest::beginFrame(const SceneManager& scenes) {
  // The starting scene only exists once the first update has run
  if (!isActive() || finished || scenes.describe().empty()) {
    return;
  }

  releaseExpiredInputs();
  if (framesToWait > 0) {
    framesToWait--;
    return;
  }

  // dump and expect take no time; every other command occupies at least this
  // frame
  while (next < commands.size()) {
    const PlaytestCommand& command = commands[next++];
    if (command.name == "dump") {
      TraceLog(LOG_INFO, "PLAYTEST: dump (line %d)", command.line);
      LogLines(LOG_INFO, scenes.describe());
      continue;
    }
    if (command.name == "expect") {
      if (scenes.describe().find(command.text) != std::string::npos) {
        continue;
      }
      TraceLog(LOG_ERROR,
               "PLAYTEST: FAILED (line %d), expected \"%s\" in:", command.line,
               command.text.c_str());
      LogLines(LOG_ERROR, scenes.describe());
      failed = true;
      finished = true;
      return;
    }
    if (command.name == "expect_exit") {
      TraceLog(LOG_ERROR,
               "PLAYTEST: FAILED (line %d), expected the game to exit",
               command.line);
      failed = true;
      finished = true;
      return;
    }

    framesToWait = command.frames - 1;
    if (command.name == "press" || command.name == "hold") {
      Inject(eventKeyDown, command.key);
      held.push_back({eventKeyUp, command.key, command.frames});
    } else if (command.name == "click" || command.name == "touch") {
      if (command.name == "touch") {
        ForceTouchControls(true);
      }
      Inject(eventMousePosition, command.x, command.y);
      Inject(eventMouseButtonDown, MOUSE_BUTTON_LEFT);
      held.push_back({eventMouseButtonUp, MOUSE_BUTTON_LEFT, command.frames});
    } else if (command.name == "shot") {
      pendingShot = command.text;
      pendingShotLine = command.line;
    } else if (command.name == "quit") {
      finished = true;
    }
    if (!held.empty()) {
      // One released frame after the input, or pressing the same key twice in a
      // row would read as a single hold
      framesToWait++;
    }
    return;
  }
  finished = true;
}

void Playtest::endFrame() {
  if (pendingShot.empty()) {
    return;
  }

  const std::filesystem::path folder =
      std::filesystem::path(pendingShot).parent_path();
  std::error_code error;
  if (!folder.empty()) {
    std::filesystem::create_directories(folder, error);
  }

  bool saved = false;
  if (!error) {
    // Drawing is batched until EndDrawing, so without this the capture would
    // miss the current frame.
    rlDrawRenderBatchActive();
    Image screen = LoadImageFromScreen();
    saved = IsImageValid(screen) && ExportImage(screen, pendingShot.c_str());
    UnloadImage(screen);
  }
  if (!saved) {
    TraceLog(
        LOG_ERROR,
        "PLAYTEST: FAILED (line %d), could not save screenshot [%s]: %s",
        pendingShotLine, pendingShot.c_str(),
        error ? error.message().c_str() : "image capture or export failed");
    failed = true;
    finished = true;
  }
  pendingShot.clear();
}

bool Playtest::finish() {
  if (!isActive()) {
    return true;
  }
  if (!finished) {
    if (next < commands.size() && commands[next].name == "expect_exit") {
      next++;
    } else {
      TraceLog(LOG_ERROR,
               "PLAYTEST: FAILED, game exited before script finished");
      if (next < commands.size()) {
        TraceLog(LOG_ERROR, "PLAYTEST: Next command was [%s] at line %d",
                 commands[next].name.c_str(), commands[next].line);
      }
      failed = true;
    }
    finished = true;
  }
  if (!failed) {
    TraceLog(LOG_INFO, "PLAYTEST: Passed");
  }
  return !failed;
}

void Playtest::releaseExpiredInputs() {
  for (size_t i = held.size(); i-- > 0;) {
    if (--held[i].framesLeft == 0) {
      Inject(held[i].releaseEvent, held[i].code);
      held.erase(held.begin() + (long)i);
    }
  }
}
