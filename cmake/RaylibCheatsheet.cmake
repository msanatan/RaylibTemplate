# Writes a Markdown cheatsheet from the fetched raylib headers, so agents can read the API for the pinned version.
function(write_raylib_cheatsheet SOURCE_DIR TAG OUTPUT)
  set(FENCE "```")

  file(READ ${SOURCE_DIR}/src/raylib.h RAYLIB_H)
  set(API_START_MARKER "// Some Basic Colors\n")
  string(FIND "${RAYLIB_H}" "${API_START_MARKER}" API_START)
  if(API_START EQUAL -1)
    message(FATAL_ERROR "RAYLIB_CHEATSHEET: could not find '${API_START_MARKER}' in raylib.h; update cmake/RaylibCheatsheet.cmake")
  endif()
  string(LENGTH "${API_START_MARKER}" MARKER_LENGTH)
  math(EXPR API_START "${API_START} + ${MARKER_LENGTH}")
  string(SUBSTRING "${RAYLIB_H}" ${API_START} -1 RAYLIB_API)

  # Preprocessor guards and the extern "C" wrapper are build plumbing, not API
  string(REGEX REPLACE "\n[ \t]*#(if|elif|else|endif|include|error|define RL_)[^\n]*" "" RAYLIB_API "${RAYLIB_API}")
  string(REGEX REPLACE "\n// Boolean type\n[ \t]+typedef enum bool[^\n]*" "" RAYLIB_API "${RAYLIB_API}")
  string(REGEX REPLACE "\nextern \"C\"[^\n]*" "" RAYLIB_API "${RAYLIB_API}")
  string(REGEX REPLACE "\n}\n" "\n" RAYLIB_API "${RAYLIB_API}")
  string(REGEX REPLACE "//-+\n// Global Variables Definition\n//-+\n[^\n]*\n" "" RAYLIB_API "${RAYLIB_API}")

  # raylib separates sections with comment banners; each becomes a heading over its own code block
  string(REGEX REPLACE "\n//-+\n// ([^\n]+)\n//-+\n" "\n${FENCE}\n\n## \\1\n\n${FENCE}c\n" RAYLIB_API "${RAYLIB_API}")
  string(REGEX REPLACE "${FENCE}c\n\n+" "${FENCE}c\n" RAYLIB_API "${RAYLIB_API}")
  string(REGEX REPLACE "\n\n+${FENCE}\n" "\n${FENCE}\n" RAYLIB_API "${RAYLIB_API}")
  string(REGEX REPLACE "\n\n\n+" "\n\n" RAYLIB_API "${RAYLIB_API}")
  string(STRIP "${RAYLIB_API}" RAYLIB_API)

  # raymath defines its functions inline, so keep each signature with the comment above it
  file(READ ${SOURCE_DIR}/src/raymath.h RAYMATH_H)
  string(REGEX MATCHALL "// Module Functions Definition - [^\n]*|//[^\n]*\nRMAPI [^\n]*" RAYMATH_ENTRIES "${RAYMATH_H}")
  set(RAYMATH_API "")
  foreach(ENTRY IN LISTS RAYMATH_ENTRIES)
    if(ENTRY MATCHES "^// Module Functions Definition - (.*)")
      if(RAYMATH_API)
        string(APPEND RAYMATH_API "${FENCE}\n\n")
      endif()
      string(APPEND RAYMATH_API "### ${CMAKE_MATCH_1}\n\n${FENCE}c\n")
    else()
      string(REGEX REPLACE "^// *([^\n]*)\n([^\n]*)" "\\2; // \\1" ENTRY "${ENTRY}")
      string(APPEND RAYMATH_API "${ENTRY}\n")
    endif()
  endforeach()
  string(APPEND RAYMATH_API "${FENCE}\n")

  file(WRITE ${OUTPUT}
    "# raylib ${TAG} API\n\n"
    "Generated from `raylib.h` and `raymath.h` when CMake configures. "
    "Do not edit; change `RAYLIB_TAG` in `CMakeLists.txt` and reconfigure instead.\n\n"
    "## Colors\n\n"
    "${FENCE}c\n"
    "${RAYLIB_API}\n"
    "${FENCE}\n\n"
    "## Math Functions (Module: raymath)\n\n"
    "${RAYMATH_API}"
  )
endfunction()
