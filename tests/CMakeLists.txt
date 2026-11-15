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
  set_tests_properties(playtest_${PLAYTEST_NAME} PROPERTIES LABELS playtest)
endforeach()
