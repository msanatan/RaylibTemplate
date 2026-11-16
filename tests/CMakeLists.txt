FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.5.3
  GIT_SHALLOW    TRUE
)
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(doctest)

file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS unit/*.cpp)
add_executable(unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE game doctest::doctest)
add_test(NAME unit COMMAND unit_tests)
set_tests_properties(unit PROPERTIES LABELS unit)

# Playtests open a window, so they are labelled separately from the unit tests that run anywhere
file(GLOB PLAYTEST_SCRIPTS CONFIGURE_DEPENDS playtests/*.txt)
foreach(PLAYTEST_SCRIPT IN LISTS PLAYTEST_SCRIPTS)
  get_filename_component(PLAYTEST_NAME ${PLAYTEST_SCRIPT} NAME_WE)
  add_test(
    NAME playtest_${PLAYTEST_NAME}
    COMMAND ${PROJECT_NAME} --script ${PLAYTEST_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  set_tests_properties(playtest_${PLAYTEST_NAME} PROPERTIES LABELS playtest TIMEOUT 30)
endforeach()

# Reset Level is bound to a key in Debug builds only
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_test(
    NAME playtest_level_reset
    COMMAND ${PROJECT_NAME} --script ${CMAKE_CURRENT_SOURCE_DIR}/playtests/debug/level_reset.txt
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  set_tests_properties(playtest_level_reset PROPERTIES LABELS playtest TIMEOUT 30)
endif()

# Protect the main-loop exit code: skipped assertions must never pass a run.
add_test(
  NAME playtest_early_exit
  COMMAND ${CMAKE_COMMAND}
    "-DGAME=$<TARGET_FILE:${PROJECT_NAME}>"
    "-DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/playtests/early_exit.txt"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckEarlyExit.cmake
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(playtest_early_exit PROPERTIES LABELS playtest TIMEOUT 30)

add_executable(level_tests ${LEVEL_TEST_SOURCES})
target_link_libraries(level_tests PRIVATE levels_runtime doctest::doctest)
add_test(NAME levels COMMAND level_tests)
set_tests_properties(levels PROPERTIES LABELS unit)

if(BUILD_EDITOR)
  add_subdirectory(${CMAKE_SOURCE_DIR}/editor/tests ${CMAKE_BINARY_DIR}/editor-tests)
endif()
