
cmake_minimum_required(VERSION 3.20)
project(RaylibTemplate LANGUAGES CXX)
set(PROJECT_NAME RaylibTemplate)
set(PROJECT_VERSION 1.0.0)

# Defaults good for jams
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Apple Silicon default
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Web (HTML5) build settings (per raylib wiki)
if(EMSCRIPTEN)
  # Ensure raylib configures for Web and Emscripten provides GLFW symbols
  set(PLATFORM Web CACHE STRING "" FORCE)
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_GLFW=3 -s WASM=1 -s GL_ENABLE_GET_PROC_ADDRESS=1")
endif()

# -------- Dependencies (FetchContent) --------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)  # Show download progress
set(RAYLIB_TAG 6.2)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        ${RAYLIB_TAG}
  GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Web preloads assets and desktop Debug reads the source folder, so only desktop releases need a pack
if(NOT EMSCRIPTEN AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PACK_ASSETS ON)
  FetchContent_Declare(miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        3.1.2
    GIT_SHALLOW    TRUE
  )
  set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
  set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(miniz)
endif()

# -------- Your game --------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
if(NOT PACK_ASSETS)
  list(FILTER SOURCES EXCLUDE REGEX "/pak\\.cpp$")
endif()
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# -------- Assets --------
set(ASSETS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/assets)
set(ASSET_IGNORED_FILES .DS_Store Thumbs.db desktop.ini)

file(GLOB_RECURSE ASSET_FILES CONFIGURE_DEPENDS RELATIVE ${ASSETS_SOURCE_DIR} ${ASSETS_SOURCE_DIR}/*)
foreach(ASSET_FILE IN LISTS ASSET_FILES)
  get_filename_component(ASSET_NAME ${ASSET_FILE} NAME)
  if(ASSET_NAME IN_LIST ASSET_IGNORED_FILES)
    list(REMOVE_ITEM ASSET_FILES ${ASSET_FILE})
  endif()
endforeach()
list(TRANSFORM ASSET_FILES PREPEND ${ASSETS_SOURCE_DIR}/ OUTPUT_VARIABLE ASSET_FILE_PATHS)

# Only rewritten when the set of assets changes, so depending on it catches deleted and renamed files
set(ASSETS_LIST_FILE ${CMAKE_BINARY_DIR}/assets.list)
list(JOIN ASSET_FILES "\n" ASSETS_LIST_CONTENT)
file(CONFIGURE OUTPUT ${ASSETS_LIST_FILE} CONTENT "${ASSETS_LIST_CONTENT}\n" @ONLY)

if(EMSCRIPTEN)
  set(WEB_SHELL_FILE ${CMAKE_BINARY_DIR}/shell.html)
  configure_file(${CMAKE_SOURCE_DIR}/web/shell.html.in ${WEB_SHELL_FILE} @ONLY)
  target_link_options(${PROJECT_NAME} PRIVATE
    --shell-file ${WEB_SHELL_FILE}
    --preload-file ${ASSETS_SOURCE_DIR}@/assets
    $<$<CONFIG:Debug>:-sASSERTIONS=1>
  )
  foreach(ASSET_NAME IN LISTS ASSET_IGNORED_FILES)
    target_link_options(${PROJECT_NAME} PRIVATE "SHELL:--exclude-file */${ASSET_NAME}")
  endforeach()
  # Emscripten reads these at link time but doesn't report them as inputs, so edits wouldn't trigger a relink
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS "${WEB_SHELL_FILE};${ASSETS_LIST_FILE};${ASSET_FILE_PATHS}")
elseif(PACK_ASSETS)
  if(APPLE)
    set(ASSETS_PAK_DIR $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources)
    set(ASSETS_PAK_PATH ../Resources/assets.pak)
  else()
    set(ASSETS_PAK_DIR $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    set(ASSETS_PAK_PATH assets.pak)
  endif()
  target_link_libraries(${PROJECT_NAME} PRIVATE miniz)
  target_compile_definitions(${PROJECT_NAME} PRIVATE "ASSETS_PAK_PATH=\"${ASSETS_PAK_PATH}\"")

  set(ASSETS_STAMP ${CMAKE_CURRENT_BINARY_DIR}/assets.stamp)
  add_custom_command(
    OUTPUT ${ASSETS_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSETS_PAK_DIR}
    COMMAND ${CMAKE_COMMAND} -E tar cf ${ASSETS_PAK_DIR}/assets.pak --format=zip --files-from=${ASSETS_LIST_FILE}
    COMMAND ${CMAKE_COMMAND} -E touch ${ASSETS_STAMP}
    WORKING_DIRECTORY ${ASSETS_SOURCE_DIR}
    DEPENDS ${ASSETS_LIST_FILE} ${ASSET_FILE_PATHS}
    COMMENT "Packing assets"
  )
  add_custom_target(pack_assets ALL DEPENDS ${ASSETS_STAMP})
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE "DEV_ASSETS_DIR=\"${ASSETS_SOURCE_DIR}/\"")
endif()

# Warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE GAME_TITLE=\"${PROJECT_NAME}\")

# On macOS: create app bundle for distribution
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.${PROJECT_NAME}.app"
    MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  )
endif()
