#include "core/ui.h"

namespace
{
const int itemFontSize = 32;
const float itemSpacing = 52.0f;
const float menuCenterFraction = 0.6f;
const Color itemColor = {200, 200, 200, 255};
const Color selectedItemColor = {255, 222, 89, 255};
} // namespace

void DrawTextCentered(const std::string &text, float screenHeightFraction, int fontSize, Color color)
{
  const int x = (GetScreenWidth() - MeasureText(text.c_str(), fontSize)) / 2;
  const int y = (int)((float)GetScreenHeight() * screenHeightFraction) - fontSize / 2;
  DrawText(text.c_str(), x, y, fontSize, color);
}

int Menu::add(const std::string &label, std::function<void()> onActivate)
{
  items.push_back({label, std::move(onActivate)});
  return (int)items.size() - 1;
}

void Menu::setLabel(int item, const std::string &label)
{
  items[item].label = label;
}

std::string Menu::selectedLabel() const
{
  return items.empty() ? "" : items[selected].label;
}

void Menu::update()
{
  if (items.empty())
  {
    return;
  }

  const int count = (int)items.size();
  if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
  {
    selected = (selected + 1) % count;
  }
  if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
  {
    selected = (selected + count - 1) % count;
  }

  bool activated = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

  const Vector2 mouse = GetMousePosition();
  const Vector2 mouseDelta = GetMouseDelta();
  const bool mouseMoved = mouseDelta.x != 0.0f || mouseDelta.y != 0.0f;
  const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  for (int i = 0; i < count; i++)
  {
    // A resting cursor must not steal the selection back from the keyboard
    if ((mouseMoved || clicked) && CheckCollisionPointRec(mouse, itemBounds(i)))
    {
      selected = i;
      activated = activated || clicked;
    }
  }

  if (activated)
  {
    items[selected].onActivate();
  }
}

void Menu::draw() const
{
  for (int i = 0; i < (int)items.size(); i++)
  {
    const Rectangle bounds = itemBounds(i);
    const Color color = i == selected ? selectedItemColor : itemColor;
    DrawText(items[i].label.c_str(), (int)bounds.x, (int)bounds.y, itemFontSize, color);
  }
}

Rectangle Menu::itemBounds(int item) const
{
  const float width = (float)MeasureText(items[item].label.c_str(), itemFontSize);
  const float top = (float)GetScreenHeight() * menuCenterFraction - itemSpacing * (float)items.size() / 2.0f;
  return {((float)GetScreenWidth() - width) / 2.0f, top + itemSpacing * (float)item, width, (float)itemFontSize};
}
