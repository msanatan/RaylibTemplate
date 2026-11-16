#include "testing/script.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "document/session.h"
#include "imgui.h"
#include "raylib.h"
#include "transport/cli.h"
#include "ui/script_targets.h"
#include "viewport/viewport.h"

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

int KeyFromName(const std::string& name) {
  static const std::map<std::string, int> named = {
      {"DELETE", KEY_DELETE},    {"BACKSPACE", KEY_BACKSPACE},
      {"ENTER", KEY_ENTER},      {"ESCAPE", KEY_ESCAPE},
      {"SUPER", KEY_LEFT_SUPER}, {"SHIFT", KEY_LEFT_SHIFT},
  };
  if (name.size() == 1 && std::isalnum((unsigned char)name[0]) != 0) {
    return std::toupper((unsigned char)name[0]);
  }
  const auto key = named.find(name);
  return key == named.end() ? KEY_NULL : key->second;
}

std::vector<std::string> Split(const std::string& text) {
  std::vector<std::string> words;
  std::istringstream stream(text);
  std::string word;
  while (stream >> word) {
    words.push_back(word);
  }
  return words;
}

void Replace(std::string& text, const std::string& from,
             const std::string& to) {
  for (size_t at = text.find(from); at != std::string::npos;
       at = text.find(from, at + to.size())) {
    text.replace(at, from.size(), to);
  }
}
}  // namespace

bool EditorScript::load(const std::string& file) {
  std::ifstream stream(file);
  if (!stream) {
    TraceLog(LOG_ERROR, "SCRIPT: Failed to open [%s]", file.c_str());
    return false;
  }
  std::string line;
  for (int number = 1; std::getline(stream, line); number++) {
    const std::string content = line.substr(0, line.find('#'));
    std::istringstream words(content);
    Command command;
    command.line = number;
    if (!(words >> command.name)) {
      continue;
    }
    std::getline(words >> std::ws, command.text);
    command.text.erase(command.text.find_last_not_of(" \t\r") + 1);
    command.words = Split(command.text);
    commands.push_back(command);
  }
  if (commands.empty()) {
    TraceLog(LOG_ERROR, "SCRIPT: [%s] contains no commands", file.c_str());
    return false;
  }
  return true;
}

void EditorScript::fail(const std::string& reason) {
  TraceLog(LOG_ERROR, "SCRIPT: FAILED (line %d): %s", currentLine,
           reason.c_str());
  failed = true;
  finished = true;
}

void EditorScript::click(float x, float y, int frames) {
  Inject(eventMousePosition, (int)x, (int)y);
  Inject(eventMouseButtonDown, MOUSE_BUTTON_LEFT);
  held.push_back({eventMouseButtonUp, MOUSE_BUTTON_LEFT, frames});
  framesToWait = frames + 2;
}

void EditorScript::startDrag(Vector2 from,
                             const std::vector<std::string>& words) {
  const auto number = [&](size_t index) {
    return (int)std::strtol(words[index].c_str(), nullptr, 10);
  };
  const int frames = std::max(number(2), 2);
  dragX = from.x;
  dragY = from.y;
  dragStepX = (float)number(0) / (float)frames;
  dragStepY = (float)number(1) / (float)frames;
  dragFrames = frames;
  // Held a little longer than the movement so the last position registers
  click(from.x, from.y, frames + 3);
}

void EditorScript::releaseExpiredInputs() {
  for (size_t i = held.size(); i-- > 0;) {
    if (--held[i].framesLeft == 0) {
      Inject(held[i].releaseEvent, held[i].code);
      held.erase(held.begin() + (long)i);
    }
  }
}

void EditorScript::beginFrame(EditorSession& session, Viewport& viewport) {
  if (!isActive() || finished) {
    return;
  }
  if (dragFrames > 0) {
    dragX += dragStepX;
    dragY += dragStepY;
    Inject(eventMousePosition, (int)dragX, (int)dragY);
    dragFrames--;
  }
  releaseExpiredInputs();
  if (framesToWait > 0) {
    framesToWait--;
    return;
  }
  if (std::chrono::steady_clock::now() < sleepUntil) {
    return;
  }

  bool continues = true;
  while (continues && next < commands.size() && !finished) {
    Command command = commands[next++];
    currentLine = command.line;
    for (const LevelObject& object : session.doc().level().objects) {
      Replace(command.text, "@" + object.name, object.id);
    }
    command.words = Split(command.text);
    continues = run(session, viewport, command);
  }
  finished = finished || (continues && next >= commands.size());
  if (finished && !failed) {
    TraceLog(LOG_INFO, "SCRIPT: Passed");
  }
}

bool EditorScript::run(EditorSession& session, Viewport& viewport,
                       const Command& command) {
  const std::vector<std::string>& words = command.words;
  const auto number = [&](size_t index) {
    return index < words.size()
               ? (int)std::strtol(words[index].c_str(), nullptr, 10)
               : 0;
  };

  if (command.name == "wait") {
    framesToWait = std::max(number(0), 1) - 1;
    return false;
  }
  if (command.name == "sleep") {
    sleepUntil =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(number(0));
    return false;
  }
  if (command.name == "quit") {
    finished = true;
    return false;
  }
  if (command.name == "expect" || command.name == "expect_not") {
    const std::string state = session.state().dump();
    const bool found = state.find(command.text) != std::string::npos;
    if (found != (command.name == "expect")) {
      fail((found ? "did not expect \"" : "expected \"") + command.text +
           "\" in " + state);
    }
    return !failed;
  }
  if (command.name == "cmd" || command.name == "cmd_fails") {
    std::vector<std::string> arguments = words;
    std::string expectedCode;
    if (command.name == "cmd_fails" && !arguments.empty()) {
      expectedCode = arguments.front();
      arguments.erase(arguments.begin());
    }
    for (std::string& argument : arguments) {
      Replace(argument, "$REV", session.doc().revision());
      Replace(argument, "$DOC", session.doc().id());
    }
    LevelJson result;
    try {
      const CliArguments parsed =
          ParseCliArguments(arguments, session.registry);
      result = session.dispatch({parsed.words, parsed.args, ""});
    } catch (const CommandFailure& failure) {
      result = FailureResult(failure);
    }
    const std::string code = result.value("code", "");
    if (expectedCode.empty() ? result["ok"] != true : code != expectedCode) {
      fail("`" + command.text + "` returned " + result.dump());
    }
    return !failed && !viewport.hasPendingScreenshot();
  }
  if (command.name == "click" && words.size() == 2) {
    click((float)number(0), (float)number(1), 1);
    return false;
  }
  if (command.name == "click_object" ||
      (command.name == "drag_object" && words.size() == 4)) {
    if (words.empty() || session.doc().index().find(words[0]) == nullptr) {
      fail("unknown object in `" + command.text + "`");
      return false;
    }
    const Vector2 position = viewport.screenPosition(words[0]);
    if (command.name == "click_object") {
      click(position.x, position.y, 1);
      return false;
    }
    startDrag(position, {words[1], words[2], words[3]});
    return false;
  }
  if (command.name == "drag_target" && words.size() == 4) {
    const auto target = ScriptTargets().find(words[0]);
    if (target == ScriptTargets().end()) {
      fail("no widget named \"" + words[0] + "\" is on screen");
      return false;
    }
    const Rectangle& area = target->second;
    startDrag(
        {area.x + std::min(area.width / 2, 40.0f), area.y + area.height / 2},
        {words[1], words[2], words[3]});
    return false;
  }
  if (command.name == "click_target") {
    const auto target = ScriptTargets().find(command.text);
    if (target == ScriptTargets().end()) {
      fail("no widget named \"" + command.text + "\" is on screen");
      return false;
    }
    const Rectangle& area = target->second;
    click(area.x + std::min(area.width / 2, 40.0f), area.y + area.height / 2,
          1);
    return false;
  }
  if (command.name == "key" && words.size() == 1) {
    std::string name = words[0];
    const size_t plus = name.find('+');
    const int modifier = plus == std::string::npos
                             ? KEY_NULL
                             : KeyFromName(name.substr(0, plus));
    const int key =
        KeyFromName(plus == std::string::npos ? name : name.substr(plus + 1));
    if (key == KEY_NULL ||
        (plus != std::string::npos && modifier == KEY_NULL)) {
      fail("unknown key \"" + name + "\"");
      return false;
    }
    if (modifier != KEY_NULL) {
      Inject(eventKeyDown, modifier);
      held.push_back({eventKeyUp, modifier, 2});
    }
    Inject(eventKeyDown, key);
    held.push_back({eventKeyUp, key, 1});
    framesToWait = 3;
    return false;
  }
  if (command.name == "type") {
    ImGui::GetIO().AddInputCharactersUTF8(command.text.c_str());
    framesToWait = 1;
    return false;
  }
  if (command.name == "copy" && words.size() == 2) {
    std::error_code error;
    std::filesystem::copy_file(
        session.project.resolve(words[0]), session.project.resolve(words[1]),
        std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
      fail("could not copy: " + error.message());
    }
    return !failed;
  }
  if (command.name == "shot" && !words.empty()) {
    viewport.requestScreenshot(words[0],
                               words.size() < 2 || words[1] != "editor");
    return false;
  }
  fail("invalid command `" + command.name + " " + command.text + "`");
  return false;
}
