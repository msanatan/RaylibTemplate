#include "ui/layout.h"

#include "imgui.h"
#include "imgui_internal.h"

void ApplyTheme(bool dark) {
  if (dark) {
    ImGui::StyleColorsDark();
  } else {
    ImGui::StyleColorsLight();
  }
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 3.0f;
  style.FramePadding = {7.0f, 4.0f};
  style.ItemSpacing = {8.0f, 6.0f};
}

void ApplyDefaultLayout(unsigned int dockspace) {
  ImGui::DockBuilderRemoveNode(dockspace);
  ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);

  ImGuiID center = dockspace;
  const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right,
                                                    0.24f, nullptr, &center);
  const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down,
                                                     0.27f, nullptr, &center);
  ImGuiID explorer = right;
  const ImGuiID properties = ImGui::DockBuilderSplitNode(
      explorer, ImGuiDir_Down, 0.6f, nullptr, &explorer);
  ImGuiID assets = bottom;
  const ImGuiID log = ImGui::DockBuilderSplitNode(assets, ImGuiDir_Right, 0.45f,
                                                  nullptr, &assets);

  ImGui::DockBuilderDockWindow(viewportWindow, center);
  ImGui::DockBuilderDockWindow(explorerWindow, explorer);
  ImGui::DockBuilderDockWindow(propertiesWindow, properties);
  ImGui::DockBuilderDockWindow(assetsWindow, assets);
  ImGui::DockBuilderDockWindow(logWindow, log);
  ImGui::DockBuilderFinish(dockspace);
}
