
cmake_minimum_required(VERSION 3.20)
project(RaylibTemplate LANGUAGES CXX)

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

# -------- Dependencies (FetchContent) --------
include(FetchContent)
set(RAYLIB_TAG 5.5)  # pin to raylib 5.5
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        ${RAYLIB_TAG}
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# -------- Your game --------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(raygame ${SOURCES})
target_include_directories(raygame PRIVATE include)
target_link_libraries(raygame PRIVATE raylib)

# Warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
  target_compile_options(raygame PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

# Assets dir macro (handy if you need it)
target_compile_definitions(raygame PRIVATE ASSETS_PATH=\"$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/assets,${CMAKE_SOURCE_DIR}/assets>\")

# On macOS: no bundle
if(APPLE)
  set_target_properties(raygame PROPERTIES MACOSX_BUNDLE FALSE)
endif()
