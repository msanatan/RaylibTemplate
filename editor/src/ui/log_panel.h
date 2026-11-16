#ifndef SCENIC_LOG_PANEL_H
#define SCENIC_LOG_PANEL_H

#include <cstdint>
#include <string>
#include <vector>

#include "commands/editor_log.h"
#include "imgui.h"
#include "raylib.h"

class LogPanel {
 public:
  void draw();

 private:
  std::vector<LogRecord> visibleRecords() const;
  void drawControls(const std::vector<LogRecord>& shown);
  void drawRecords(const std::vector<LogRecord>& shown);
  void copy(const std::vector<LogRecord>& shown, bool selectedOnly) const;

  int minimumSeverity = LOG_INFO;
  bool autoFollow = true;
  bool followRequested = false;
  bool showEngineDetail = false;
  uint64_t clearedThrough = 0;
  // Keyed by record sequence, so the selection survives new records
  ImGuiSelectionBasicStorage selection;
};

#endif  // !SCENIC_LOG_PANEL_H
