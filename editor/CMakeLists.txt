FetchContent_Declare(tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG        v3.4.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

# Everything that needs no window: the document, history, live commands, and the local socket. The
# windowless integration tests and scenic-cli link this without any UI dependency.
add_library(scenic_core STATIC
  src/transport/command_server.cpp
  src/transport/live_requests.cpp
  src/commands/live_commands.cpp
  src/document/document.cpp
  src/document/history.cpp
  src/commands/editor_log.cpp
  src/document/project.cpp
  src/document/selection.cpp
  src/document/session.cpp
  src/transport/cli.cpp
)
target_include_directories(scenic_core PUBLIC src)
target_link_libraries(scenic_core PUBLIC levels_editing raylib tomlplusplus::tomlplusplus)
target_compile_options(scenic_core PRIVATE ${WARNING_FLAGS})

add_executable(scenic-cli src/transport/cli_main.cpp)
set_target_properties(scenic-cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(scenic-cli PRIVATE scenic_core)
target_compile_options(scenic-cli PRIVATE ${WARNING_FLAGS})

# -------- Editor UI dependencies --------
# None of these ship a CMake build, so their sources are compiled here
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.92.9b-docking
  GIT_SHALLOW    TRUE
)
FetchContent_Declare(rlimgui
  GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
  GIT_TAG        5c17e70e98ae9f50ca94bdcad0e3b3536b46a203
)
FetchContent_Declare(imguizmo
  GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
  GIT_TAG        105e4fde45e4e74aae8973d98750820ef7aca7e2
  # Its own CMake build expects ImGui to be installed; only the sources are used
  SOURCE_SUBDIR  not-built
)
FetchContent_MakeAvailable(imgui rlimgui imguizmo)

add_library(editor_ui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${rlimgui_SOURCE_DIR}/rlImGui.cpp
  ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
)
target_include_directories(editor_ui SYSTEM PUBLIC
  ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/misc/cpp ${rlimgui_SOURCE_DIR} ${imguizmo_SOURCE_DIR}/src
)
target_link_libraries(editor_ui PUBLIC raylib)
target_compile_options(editor_ui PRIVATE -w)
# Scripted runs open an unfocused window, and rlImGui otherwise ignores the mouse without focus
target_compile_definitions(editor_ui PRIVATE RLIMGUI_ALWAYS_TRACK_MOUSE)

# -------- Scenic --------
add_executable(Scenic
  src/main.cpp
  src/ui/actions.cpp
  src/ui/dialogs.cpp
  src/ui/layout.cpp
  src/ui/menu_bar.cpp
  src/ui/script_targets.cpp
  src/ui/shortcuts.cpp
  src/ui/toolbar.cpp
  src/app/editor_app.cpp
  src/app/file_watch.cpp
  src/app/preferences.cpp
  src/app/process.cpp
  src/testing/script.cpp
  src/ui/assets_panel.cpp
  src/ui/explorer_panel.cpp
  src/ui/log_panel.cpp
  src/ui/properties_panel.cpp
  src/viewport/camera.cpp
  src/viewport/gizmo.cpp
  src/viewport/picking.cpp
  src/viewport/viewport.cpp
)
set_target_properties(Scenic PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(Scenic PRIVATE scenic_core levels_runtime editor_ui)
target_compile_options(Scenic PRIVATE ${WARNING_FLAGS})
# Play and Reset launch the game built alongside the editor unless project.toml names another
target_compile_definitions(Scenic PRIVATE
  "SCENIC_DEFAULT_PROJECT=\"${CMAKE_SOURCE_DIR}/game\""
  "SCENIC_DEFAULT_GAME=\"$<TARGET_FILE:${PROJECT_NAME}>\""
)
add_dependencies(Scenic ${PROJECT_NAME})

add_subdirectory(tests)
