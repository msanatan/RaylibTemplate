#include "core/playtest.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "raylib.h"
#include "rlgl.h"

namespace
{
// raylib's AutomationEventType values, which rcore.c keeps private
const int eventKeyUp = 1;
const int eventKeyDown = 2;
const int eventMouseButtonUp = 5;
const int eventMouseButtonDown = 6;
const int eventMousePosition = 7;

void Inject(int event, int first, int second = 0)
{
  PlayAutomationEvent({0, (unsigned int)event, {first, second, 0, 0}});
}

void LogLines(int level, const std::string &lines)
{
  std::istringstream stream(lines);
  std::string line;
  while (std::getline(stream, line))
  {
    TraceLog(level, "PLAYTEST:   %s", line.c_str());
  }
}

int KeyFromName(const std::string &name)
{
  static const std::map<std::string, int> namedKeys = {
      {"SPACE", KEY_SPACE},
      {"ENTER", KEY_ENTER},
      {"ESCAPE", KEY_ESCAPE},
      {"TAB", KEY_TAB},
      {"BACKSPACE", KEY_BACKSPACE},
      {"UP", KEY_UP},
      {"DOWN", KEY_DOWN},
      {"LEFT", KEY_LEFT},
      {"RIGHT", KEY_RIGHT},
      {"SHIFT", KEY_LEFT_SHIFT},
      {"CONTROL", KEY_LEFT_CONTROL},
  };

  if (name.size() == 1 && std::isalnum((unsigned char)name[0]))
  {
    return std::toupper((unsigned char)name[0]);
  }
  const auto key = namedKeys.find(name);
  return key == namedKeys.end() ? KEY_NULL : key->second;
}
} // namespace

bool Playtest::load(const std::string &scriptFile)
{
  std::ifstream file(scriptFile);
  if (!file)
  {
    TraceLog(LOG_ERROR, "PLAYTEST: Failed to open [%s]", scriptFile.c_str());
    return false;
  }

  std::string line;
  for (int lineNumber = 1; std::getline(file, line); lineNumber++)
  {
    std::istringstream words(line.substr(0, line.find('#')));
    Command command;
    command.line = lineNumber;
    if (!(words >> command.name))
    {
      continue;
    }

    std::string keyName;
    bool valid = true;
    if (command.name == "press")
    {
      valid = (bool)(words >> keyName);
    }
    else if (command.name == "hold")
    {
      valid = (bool)(words >> keyName >> command.frames);
    }
    else if (command.name == "click")
    {
      valid = (bool)(words >> command.x >> command.y);
    }
    else if (command.name == "wait")
    {
      valid = (bool)(words >> command.frames);
    }
    else if (command.name == "shot" || command.name == "expect")
    {
      std::getline(words >> std::ws, command.text);
      command.text.erase(command.text.find_last_not_of(" \t\r") + 1);
      valid = !command.text.empty();
    }
    else if (command.name != "dump" && command.name != "quit")
    {
      valid = false;
    }

    command.key = KeyFromName(keyName);
    if (!valid || command.frames < 1 || (!keyName.empty() && command.key == KEY_NULL))
    {
      TraceLog(LOG_ERROR, "PLAYTEST: [%s:%d] Invalid command: %s", scriptFile.c_str(), lineNumber, line.c_str());
      return false;
    }
    commands.push_back(command);
  }
  return true;
}

void Playtest::beginFrame(const SceneManager &scenes)
{
  // The starting scene only exists once the first update has run
  if (!isActive() || finished || scenes.describe().empty())
  {
    return;
  }

  releaseExpiredInputs();
  if (framesToWait > 0)
  {
    framesToWait--;
    return;
  }

  // dump and expect take no time; every other command occupies at least this frame
  while (next < commands.size())
  {
    const Command &command = commands[next++];
    if (command.name == "dump")
    {
      TraceLog(LOG_INFO, "PLAYTEST: dump (line %d)", command.line);
      LogLines(LOG_INFO, scenes.describe());
      continue;
    }
    if (command.name == "expect")
    {
      if (scenes.describe().find(command.text) != std::string::npos)
      {
        continue;
      }
      TraceLog(LOG_ERROR, "PLAYTEST: FAILED (line %d), expected \"%s\" in:", command.line, command.text.c_str());
      LogLines(LOG_ERROR, scenes.describe());
      failed = true;
      finished = true;
      return;
    }

    framesToWait = command.frames - 1;
    if (command.name == "press" || command.name == "hold")
    {
      Inject(eventKeyDown, command.key);
      held.push_back({eventKeyUp, command.key, command.frames});
    }
    else if (command.name == "click")
    {
      Inject(eventMousePosition, command.x, command.y);
      Inject(eventMouseButtonDown, MOUSE_BUTTON_LEFT);
      held.push_back({eventMouseButtonUp, MOUSE_BUTTON_LEFT, 1});
    }
    else if (command.name == "shot")
    {
      pendingShot = command.text;
    }
    else if (command.name == "quit")
    {
      finished = true;
    }
    if (!held.empty())
    {
      // One released frame after the input, or pressing the same key twice in a row would read as a single hold
      framesToWait++;
    }
    return;
  }
  finished = true;
}

void Playtest::endFrame()
{
  if (pendingShot.empty())
  {
    return;
  }

  // Drawing is batched until EndDrawing, so without this the capture would miss the current frame
  rlDrawRenderBatchActive();
  Image screen = LoadImageFromScreen();
  const std::filesystem::path folder = std::filesystem::path(pendingShot).parent_path();
  if (!folder.empty())
  {
    std::filesystem::create_directories(folder);
  }
  ExportImage(screen, pendingShot.c_str());
  UnloadImage(screen);
  pendingShot.clear();
}

void Playtest::releaseExpiredInputs()
{
  for (size_t i = held.size(); i-- > 0;)
  {
    if (--held[i].framesLeft == 0)
    {
      Inject(held[i].releaseEvent, held[i].code);
      held.erase(held.begin() + (long)i);
    }
  }
}
