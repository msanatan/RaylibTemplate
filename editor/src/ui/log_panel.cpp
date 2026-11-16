#include "ui/log_panel.h"

namespace {
ImVec4 SeverityColor(int level) {
  if (level >= LOG_ERROR) {
    return {1.0f, 0.42f, 0.42f, 1.0f};
  }
  if (level == LOG_WARNING) {
    return {1.0f, 0.8f, 0.3f, 1.0f};
  }
  return ImGui::GetStyleColorVec4(level == LOG_INFO ? ImGuiCol_Text
                                                    : ImGuiCol_TextDisabled);
}

bool IsEngineDetail(const LogRecord& record) {
  const bool ours =
      record.subsystem == "SCENIC" || record.subsystem == "LEVEL" ||
      record.subsystem == "ASSETS" || record.subsystem == "SCRIPT";
  return !ours && record.level < LOG_WARNING;
}

ImGuiID SequenceOfRow(ImGuiSelectionBasicStorage* storage, int row) {
  const auto* shown =
      static_cast<const std::vector<LogRecord>*>(storage->UserData);
  return (ImGuiID)(*shown)[(size_t)row].sequence;
}
}  // namespace

std::vector<LogRecord> LogPanel::visibleRecords() const {
  std::vector<LogRecord> shown;
  for (const LogRecord& record : LogRecordsAfter(clearedThrough)) {
    if (record.level >= minimumSeverity &&
        (showEngineDetail || !IsEngineDetail(record))) {
      shown.push_back(record);
    }
  }
  return shown;
}

void LogPanel::copy(const std::vector<LogRecord>& shown,
                    bool selectedOnly) const {
  std::string text;
  for (const LogRecord& record : shown) {
    if (!selectedOnly || selection.Contains((ImGuiID)record.sequence)) {
      text += std::string(LogSeverityName(record.level)) + "  " +
              record.message + "\n";
    }
  }
  ImGui::SetClipboardText(text.c_str());
}

void LogPanel::drawControls(const std::vector<LogRecord>& shown) {
  ImGui::SetNextItemWidth(110);
  if (ImGui::BeginCombo("##severity", LogSeverityName(minimumSeverity))) {
    for (const int level :
         {LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR}) {
      if (ImGui::Selectable(LogSeverityName(level), level == minimumSeverity)) {
        minimumSeverity = level;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SetItemTooltip("Show this severity and above");
  ImGui::SameLine();
  if (ImGui::Button("Clear")) {
    clearedThrough = NewestLogSequence();
    selection.Clear();
  }
  ImGui::SameLine();
  if (ImGui::Button("Copy")) {
    copy(shown, selection.Size > 0);
  }
  ImGui::SetItemTooltip("Copy the selected rows, or everything shown");
  ImGui::SameLine();
  followRequested = ImGui::Checkbox("Follow", &autoFollow) && autoFollow;
  ImGui::SameLine();
  ImGui::Checkbox("Engine", &showEngineDetail);
  ImGui::SetItemTooltip("Also show raylib's own informational messages");
  ImGui::Separator();
}

void LogPanel::drawRecords(const std::vector<LogRecord>& shown) {
  ImGui::BeginChild("records", {0, 0}, ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar);
  // Read before the rows are drawn: someone at the bottom stays there,
  // someone reading older output is left alone
  const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

  selection.UserData = (void*)&shown;
  selection.AdapterIndexToStorageId = SequenceOfRow;
  const ImGuiMultiSelectFlags flags = ImGuiMultiSelectFlags_ClearOnEscape |
                                      ImGuiMultiSelectFlags_ClearOnClickVoid |
                                      ImGuiMultiSelectFlags_BoxSelect1d;
  selection.ApplyRequests(
      ImGui::BeginMultiSelect(flags, selection.Size, (int)shown.size()));
  for (size_t row = 0; row < shown.size(); row++) {
    const LogRecord& record = shown[row];
    ImGui::PushID((int)record.sequence);
    ImGui::PushStyleColor(ImGuiCol_Text, SeverityColor(record.level));
    ImGui::SetNextItemSelectionUserData((ImGuiSelectionUserData)row);
    ImGui::Selectable(record.message.c_str(),
                      selection.Contains((ImGuiID)record.sequence));
    ImGui::PopStyleColor();
    ImGui::PopID();
  }
  selection.ApplyRequests(ImGui::EndMultiSelect());

  const bool focused =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  if (focused && selection.Size > 0 &&
      ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C)) {
    copy(shown, true);
  }
  if (ImGui::BeginPopupContextWindow("log-menu")) {
    if (ImGui::MenuItem("Copy", "Cmd/Ctrl+C", false, selection.Size > 0)) {
      copy(shown, true);
    }
    if (ImGui::MenuItem("Copy All")) {
      copy(shown, false);
    }
    ImGui::EndPopup();
  }

  if (autoFollow && (atBottom || followRequested)) {
    ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();
}

void LogPanel::draw() {
  const std::vector<LogRecord> shown = visibleRecords();
  drawControls(shown);
  drawRecords(shown);
}
