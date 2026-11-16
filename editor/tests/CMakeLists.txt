# Windowless: scenic-cli, the editor's document layer, and its local socket
file(GLOB INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS integration/*.cpp)
add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})
target_link_libraries(integration_tests PRIVATE scenic_core doctest::doctest)
target_compile_options(integration_tests PRIVATE ${WARNING_FLAGS})
add_test(NAME integration COMMAND integration_tests)
set_tests_properties(integration PROPERTIES LABELS integration TIMEOUT 60)

# Windowed: each script drives the real editor against a fresh copy of the fixture project
file(GLOB EDITOR_SCRIPTS CONFIGURE_DEPENDS scripts/*.txt)
foreach(EDITOR_SCRIPT IN LISTS EDITOR_SCRIPTS)
  get_filename_component(EDITOR_TEST_NAME ${EDITOR_SCRIPT} NAME_WE)
  add_test(
    NAME editor_${EDITOR_TEST_NAME}
    COMMAND ${CMAKE_COMMAND}
      "-DEDITOR=$<TARGET_FILE:Scenic>"
      "-DSCRIPT=${EDITOR_SCRIPT}"
      "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/project"
      "-DPROJECT=${CMAKE_BINARY_DIR}/editor_tests/${EDITOR_TEST_NAME}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/RunEditorScript.cmake
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  set_tests_properties(editor_${EDITOR_TEST_NAME} PROPERTIES LABELS editor TIMEOUT 60)
endforeach()
