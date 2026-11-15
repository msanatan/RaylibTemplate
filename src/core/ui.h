#ifndef GBM_UI_H
#define GBM_UI_H

#include <functional>
#include <string>
#include <vector>

#include "raylib.h"

void DrawTextCentered(const std::string &text, float screenHeightFraction, int fontSize, Color color);

// Vertical list driven by arrows/WASD + Enter/Space, or by mouse and touch
class Menu
{
public:
  int add(const std::string &label, std::function<void()> onActivate);
  void setLabel(int item, const std::string &label);

  void update();
  void draw() const;

private:
  struct Item
  {
    std::string label;
    std::function<void()> onActivate;
  };

  Rectangle itemBounds(int item) const;

  std::vector<Item> items;
  int selected = 0;
};

#endif // !GBM_UI_H
