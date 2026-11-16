# -------- Your game --------
# Everything except main.cpp is a library, so the unit tests link the same code the game runs
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER SOURCES EXCLUDE REGEX "/src/main\\.cpp$")
if(NOT PACK_ASSETS)
  list(FILTER SOURCES EXCLUDE REGEX "/pak\\.cpp$")
endif()
add_library(game STATIC ${SOURCES})
target_include_directories(game PUBLIC src)
target_link_libraries(game PUBLIC raylib levels_runtime)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE game)

# -------- Assets --------
include(cmake/Assets.cmake)

target_compile_options(game PUBLIC ${WARNING_FLAGS})

target_compile_definitions(game PUBLIC GAME_TITLE=\"${PROJECT_NAME}\")

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

if(NOT EMSCRIPTEN)
  add_subdirectory(tests)
endif()
