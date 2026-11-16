#ifndef SCENIC_MENU_BAR_H
#define SCENIC_MENU_BAR_H

#include "app/preferences.h"
#include "ui/context.h"

void DrawMenuBar(const UiContext& ui, Preferences& preferences,
                 bool& resetLayout);

#endif  // !SCENIC_MENU_BAR_H
