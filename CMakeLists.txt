
cmake_minimum_required(VERSION 3.20)
project(RaylibTemplate LANGUAGES CXX)
set(PROJECT_NAME RaylibTemplate)

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
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s GL_ENABLE_GET_PROC_ADDRESS=1")
endif()

# -------- Dependencies (FetchContent) --------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)  # Show download progress
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
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# Warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

# Assets dir macro (handy if you need it)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_PATH=\"$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/assets,${CMAKE_SOURCE_DIR}/assets>\")

# On macOS: no bundle
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE FALSE)
endif()
