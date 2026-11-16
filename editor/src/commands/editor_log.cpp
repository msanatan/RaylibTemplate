#include "commands/editor_log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>

#include "raylib.h"

namespace {
const std::chrono::steady_clock::time_point launch =
    std::chrono::steady_clock::now();
std::mutex mutex;
std::FILE* file = nullptr;
std::deque<LogRecord> records;
uint64_t nextSequence = 1;
std::string currentRequest;

std::string Subsystem(const std::string& message) {
  const size_t colon = message.find(": ");
  const bool prefixed =
      colon != std::string::npos && colon > 0 && colon <= 16 &&
      message.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ_") == colon;
  return prefixed ? message.substr(0, colon) : "";
}

void Write(int level, const char* format, va_list arguments) {
  va_list measuring;
  va_copy(measuring, arguments);
  std::string message((size_t)std::vsnprintf(nullptr, 0, format, measuring),
                      '\0');
  va_end(measuring);
  std::vsnprintf(message.data(), message.size() + 1, format, arguments);

  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - launch)
          .count();
  {
    const std::lock_guard<std::mutex> lock(mutex);
    for (std::FILE* output : {stdout, file}) {
      if (output != nullptr) {
        std::fprintf(output, "[%8.3f %-7s] %s\n", seconds,
                     LogSeverityName(level), message.c_str());
        std::fflush(output);
      }
    }
    records.push_back(
        {nextSequence++, level, Subsystem(message), message, currentRequest});
    if (records.size() > logRetention) {
      records.pop_front();
    }
  }

  // raylib only exits on LOG_FATAL itself when no callback is installed
  if (level == LOG_FATAL) {
    std::exit(EXIT_FAILURE);
  }
}
}  // namespace

void StartEditorLog(const std::string& logFile) {
  if (!logFile.empty()) {
    file = std::fopen(logFile.c_str(), "w");
  }
  SetTraceLogCallback(Write);
}

void StopEditorLog() {
  SetTraceLogCallback(nullptr);
  const std::lock_guard<std::mutex> lock(mutex);
  if (file != nullptr) {
    std::fclose(file);
    file = nullptr;
  }
}

std::vector<LogRecord> LogRecordsAfter(uint64_t sequence) {
  const std::lock_guard<std::mutex> lock(mutex);
  std::vector<LogRecord> result;
  for (const LogRecord& record : records) {
    if (record.sequence > sequence) {
      result.push_back(record);
    }
  }
  return result;
}

uint64_t OldestLogSequence() {
  const std::lock_guard<std::mutex> lock(mutex);
  return records.empty() ? nextSequence : records.front().sequence;
}

uint64_t NewestLogSequence() {
  const std::lock_guard<std::mutex> lock(mutex);
  return nextSequence - 1;
}

void SetLogRequestId(const std::string& requestId) {
  const std::lock_guard<std::mutex> lock(mutex);
  currentRequest = requestId;
}

const char* LogSeverityName(int level) {
  switch (level) {
    case LOG_TRACE:
      return "trace";
    case LOG_DEBUG:
      return "debug";
    case LOG_WARNING:
      return "warning";
    case LOG_ERROR:
      return "error";
    case LOG_FATAL:
      return "fatal";
    default:
      return "info";
  }
}
