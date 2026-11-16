#ifndef SCENIC_ASSETS_PANEL_H
#define SCENIC_ASSETS_PANEL_H

#include <filesystem>
#include <map>
#include <string>

#include "raylib.h"
#include "ui/context.h"

class AssetsPanel {
 public:
  AssetsPanel() = default;
  ~AssetsPanel();
  AssetsPanel(const AssetsPanel&) = delete;
  AssetsPanel& operator=(const AssetsPanel&) = delete;

  void draw(const UiContext& ui);
  // Call when image files change so thumbnails load again
  void unloadThumbnails();

 private:
  void drawBreadcrumbs();
  void drawEntry(const UiContext& ui,
                 const std::filesystem::directory_entry& entry);
  const Texture2D* thumbnail(const std::filesystem::path& file);

  std::string folder;
  std::map<std::string, Texture2D> thumbnails;
};

#endif  // !SCENIC_ASSETS_PANEL_H
