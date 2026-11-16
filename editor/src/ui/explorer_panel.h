#ifndef SCENIC_EXPLORER_PANEL_H
#define SCENIC_EXPLORER_PANEL_H

#include <optional>
#include <string>

#include "ui/context.h"

class ExplorerPanel {
 public:
  void draw(const UiContext& ui);

 private:
  void drawRow(const UiContext& ui, const std::string& id);
  void drawRowMenu(const UiContext& ui, const std::string& id);
  void drawCreateItems(const UiContext& ui,
                       const std::optional<std::string>& parent);
};

#endif  // !SCENIC_EXPLORER_PANEL_H
