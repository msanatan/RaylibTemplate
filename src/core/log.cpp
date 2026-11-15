#include "core/log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "raylib.h"

namespace
{
const std::chrono::steady_clock::time_point launch = std::chrono::steady_clock::now();
std::FILE *file = nullptr;
int frame = 0;
std::string currentStack;
std::string topScene = "-";

void WriteLine(int level, const char *format, va_list arguments)
{
  static const char *const levelNames[] = {"ALL", "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

  va_list measuring;
  va_copy(measuring, arguments);
  std::string message((size_t)std::vsnprintf(nullptr, 0, format, measuring), '\0');
  va_end(measuring);
  std::vsnprintf(message.data(), message.size() + 1, format, arguments);

  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - launch).count();
  for (std::FILE *output : {stdout, file})
  {
    if (output != nullptr)
    {
      std::fprintf(output, "[%8.3f f%d %-5s %s] %s\n", seconds, frame, levelNames[level], topScene.c_str(), message.c_str());
      std::fflush(output);
    }
  }

  // raylib only exits on LOG_FATAL itself when no callback is installed
  if (level == LOG_FATAL)
  {
    std::exit(EXIT_FAILURE);
  }
}
} // namespace

void InitLog(const std::string &logFile)
{
  if (!logFile.empty())
  {
    file = std::fopen(logFile.c_str(), "w");
  }
  SetTraceLogCallback(WriteLine);
}

void CloseLog()
{
  SetTraceLogCallback(nullptr);
  if (file != nullptr)
  {
    std::fclose(file);
    file = nullptr;
  }
}

void LogNextFrame()
{
  frame++;
}

void LogSceneStack(const std::string &sceneStack)
{
  if (sceneStack == currentStack)
  {
    return;
  }
  currentStack = sceneStack;
  const size_t separator = sceneStack.rfind(' ');
  topScene = sceneStack.empty() ? "-" : sceneStack.substr(separator == std::string::npos ? 0 : separator + 1);
  TraceLog(LOG_INFO, "SCENE: %s", sceneStack.empty() ? "(none)" : sceneStack.c_str());
}
