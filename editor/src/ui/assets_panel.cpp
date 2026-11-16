#include "ui/assets_panel.h"

#include <algorithm>
#include <vector>

#include "document/session.h"
#include "imgui.h"
#include "rlImGui.h"
#include "ui/dialogs.h"
#include "ui/properties_panel.h"

namespace {
namespace fs = std::filesystem;

const float tileSize = 84.0f;
const int thumbnailPixels = 128;
const size_t maximumThumbnails = 256;

bool IsImage(const fs::path& file) {
  std::string extension = file.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
         extension == ".bmp" || extension == ".tga";
}

std::vector<fs::directory_entry> VisibleEntries(const fs::path& directory) {
  std::vector<fs::directory_entry> entries;
  std::error_code error;
  for (const fs::directory_entry& entry :
       fs::directory_iterator(directory, error)) {
    if (entry.path().filename().string().front() != '.') {
      entries.push_back(entry);
    }
  }
  std::sort(entries.begin(), entries.end(),
            [](const fs::directory_entry& a, const fs::directory_entry& b) {
              if (a.is_directory() != b.is_directory()) {
                return a.is_directory();
              }
              return a.path().filename() < b.path().filename();
            });
  return entries;
}

void ApplyTexture(EditorSession& session, const std::string& reference) {
  const LevelObject* object =
      session.doc().index().find(session.selection.primary());
  if (object != nullptr && IsPrimitive(object->kind)) {
    session.edit(
        {{"op", "set-appearance"}, {"id", object->id}, {"texture", reference}},
        "Set Texture");
  }
}
}  // namespace

AssetsPanel::~AssetsPanel() { unloadThumbnails(); }

void AssetsPanel::unloadThumbnails() {
  for (const auto& [path, texture] : thumbnails) {
    if (IsTextureValid(texture)) {
      UnloadTexture(texture);
    }
  }
  thumbnails.clear();
}

const Texture2D* AssetsPanel::thumbnail(const fs::path& file) {
  const std::string key = file.string();
  const auto existing = thumbnails.find(key);
  if (existing != thumbnails.end()) {
    return IsTextureValid(existing->second) ? &existing->second : nullptr;
  }
  if (thumbnails.size() >= maximumThumbnails) {
    return nullptr;
  }
  Image image = LoadImage(key.c_str());
  Texture2D texture = {};
  if (IsImageValid(image)) {
    const float scale =
        std::min(1.0f, (float)thumbnailPixels /
                           (float)std::max(image.width, image.height));
    ImageResize(&image, std::max((int)((float)image.width * scale), 1),
                std::max((int)((float)image.height * scale), 1));
    texture = LoadTextureFromImage(image);
    UnloadImage(image);
  }
  thumbnails[key] = texture;
  return IsTextureValid(texture) ? &thumbnails[key] : nullptr;
}

void AssetsPanel::drawBreadcrumbs() {
  if (ImGui::SmallButton("assets")) {
    folder.clear();
  }
  fs::path walked;
  for (const fs::path& part : fs::path(folder)) {
    walked /= part;
    ImGui::SameLine();
    ImGui::TextUnformatted("/");
    ImGui::SameLine();
    if (ImGui::SmallButton(part.string().c_str())) {
      folder = walked.generic_string();
    }
  }
  ImGui::Separator();
}

void AssetsPanel::drawEntry(const UiContext& ui,
                            const fs::directory_entry& entry) {
  const std::string name = entry.path().filename().string();
  const std::string reference = (fs::path(folder) / name).generic_string();
  const bool isFolder = entry.is_directory();
  const bool isImage = !isFolder && IsImage(entry.path());
  const bool isLevel = !isFolder && entry.path().extension() == ".json";

  ImGui::PushID(name.c_str());
  const Texture2D* image = isImage ? thumbnail(entry.path()) : nullptr;
  const bool clicked =
      image != nullptr
          ? rlImGuiImageButtonSize("tile", image, {tileSize, tileSize})
          : ImGui::Button(isFolder  ? "Folder"
                          : isLevel ? "Level"
                                    : "File",
                          {tileSize + 8, tileSize + 8});

  if (isImage && ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload(assetDragPayload, reference.c_str(),
                              reference.size() + 1);
    ImGui::TextUnformatted(name.c_str());
    ImGui::EndDragDropSource();
  }
  if (isImage) {
    ImGui::SetItemTooltip(
        "Click to apply to the selected object, or drag onto a Texture field");
  } else if (isLevel) {
    ImGui::SetItemTooltip("Double-click to open this level");
  }

  if (isFolder && clicked) {
    folder = reference;
  } else if (isImage && clicked) {
    ApplyTexture(ui.session, reference);
  } else if (isLevel && ImGui::IsItemHovered() &&
             ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    ui.dialogs.requestOpen(ui.session, entry.path().string());
  }
  ImGui::TextWrapped("%s", name.c_str());
  ImGui::PopID();
}

void AssetsPanel::draw(const UiContext& ui) {
  const fs::path root = ui.session.project.assetsDirectory;
  std::error_code error;
  if (!fs::is_directory(root / folder, error)) {
    folder.clear();
  }
  drawBreadcrumbs();

  // Copied because opening a folder changes `folder` mid-loop
  const std::vector<fs::directory_entry> entries =
      VisibleEntries(root / folder);
  ImGui::BeginChild("files");
  if (entries.empty()) {
    ImGui::TextDisabled("This folder is empty");
  }
  const int columns =
      std::max(1, (int)(ImGui::GetContentRegionAvail().x / (tileSize + 16)));
  if (ImGui::BeginTable("grid", columns)) {
    for (const fs::directory_entry& entry : entries) {
      ImGui::TableNextColumn();
      drawEntry(ui, entry);
    }
    ImGui::EndTable();
  }
  ImGui::EndChild();
}
