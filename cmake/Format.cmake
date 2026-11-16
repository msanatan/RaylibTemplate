# Usage: cmake -P cmake/Format.cmake              rewrites files in place
#        cmake -DCHECK=ON -P cmake/Format.cmake   fails if any file would change
#
# The version is pinned and fetched through uv so that every machine and CI format identically.
set(CLANG_FORMAT_VERSION 23.1.5)

get_filename_component(PROJECT_ROOT ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
file(GLOB_RECURSE FORMAT_FILES
  ${PROJECT_ROOT}/src/*.cpp ${PROJECT_ROOT}/src/*.h
  ${PROJECT_ROOT}/tests/*.cpp ${PROJECT_ROOT}/tests/*.h
  ${PROJECT_ROOT}/levels/*.cpp ${PROJECT_ROOT}/levels/*.h
  ${PROJECT_ROOT}/editor/src/*.cpp ${PROJECT_ROOT}/editor/src/*.h
  ${PROJECT_ROOT}/editor/tests/*.cpp ${PROJECT_ROOT}/editor/tests/*.h
)

if(CHECK)
  set(FORMAT_MODE --dry-run --Werror)
else()
  set(FORMAT_MODE -i)
endif()

execute_process(
  COMMAND uvx --from clang-format==${CLANG_FORMAT_VERSION} clang-format ${FORMAT_MODE} ${FORMAT_FILES}
  RESULT_VARIABLE FORMAT_RESULT
)
if(NOT FORMAT_RESULT EQUAL 0)
  message(FATAL_ERROR "Formatting failed. Run `cmake -P cmake/Format.cmake` to fix the files listed above.")
endif()
