#ifndef SCENIC_LAYOUT_H
#define SCENIC_LAYOUT_H

inline constexpr const char* viewportWindow = "Viewport";
inline constexpr const char* explorerWindow = "Explorer";
inline constexpr const char* propertiesWindow = "Properties";
inline constexpr const char* assetsWindow = "Assets";
inline constexpr const char* logWindow = "Log";

void ApplyTheme(bool dark);
void ApplyDefaultLayout(unsigned int dockspace);

#endif  // !SCENIC_LAYOUT_H
