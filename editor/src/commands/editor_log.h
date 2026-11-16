#ifndef SCENIC_EDITOR_LOG_H
#define SCENIC_EDITOR_LOG_H

#include <cstdint>
#include <string>
#include <vector>

struct LogRecord {
  uint64_t sequence = 0;
  int level = 0;
  std::string subsystem;
  std::string message;
  std::string requestId;
};

inline constexpr size_t logRetention = 2000;

void StartEditorLog(const std::string& logFile);
void StopEditorLog();

std::vector<LogRecord> LogRecordsAfter(uint64_t sequence);
uint64_t OldestLogSequence();
uint64_t NewestLogSequence();

void SetLogRequestId(const std::string& requestId);
const char* LogSeverityName(int level);

#endif  // !SCENIC_EDITOR_LOG_H
