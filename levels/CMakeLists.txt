# Authored 3D levels, shared by the game and the Scenic editor so that neither owns the format.
# Split so that tools which only edit documents never link a graphics library:
#   levels          document/  data model, JSON reading and writing, validation
#   levels_runtime  runtime/   loading and drawing levels with raylib
#   levels_editing  editing/   edit operations and the command interface (desktop tools only)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.12.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

file(GLOB LEVEL_DOCUMENT_SOURCES CONFIGURE_DEPENDS document/*.cpp)
file(GLOB LEVEL_RUNTIME_SOURCES CONFIGURE_DEPENDS runtime/*.cpp)
set(LEVEL_SOURCES ${LEVEL_DOCUMENT_SOURCES} ${LEVEL_RUNTIME_SOURCES})

add_library(levels STATIC ${LEVEL_DOCUMENT_SOURCES})
# Includes are written "levels/document/level.h"
target_include_directories(levels PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(levels PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(levels PRIVATE ${WARNING_FLAGS})

add_library(levels_runtime STATIC ${LEVEL_RUNTIME_SOURCES})
target_link_libraries(levels_runtime PUBLIC levels raylib)
target_compile_options(levels_runtime PRIVATE ${WARNING_FLAGS})

if(NOT EMSCRIPTEN)
  file(GLOB LEVEL_EDITING_SOURCES CONFIGURE_DEPENDS editing/*.cpp)
  list(APPEND LEVEL_SOURCES ${LEVEL_EDITING_SOURCES})
  add_library(levels_editing STATIC ${LEVEL_EDITING_SOURCES})
  target_link_libraries(levels_editing PUBLIC levels)
  target_compile_options(levels_editing PRIVATE ${WARNING_FLAGS})

  add_subdirectory(tests)
  file(GLOB LEVEL_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
  list(APPEND LEVEL_SOURCES ${LEVEL_TEST_SOURCES})
endif()
set(LEVEL_SOURCES ${LEVEL_SOURCES} PARENT_SCOPE)
