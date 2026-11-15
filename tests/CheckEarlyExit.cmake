# An unexpected game exit must fail the run; a crash or timeout is not a pass.
execute_process(
  COMMAND "${GAME}" --scene main_menu --script "${SCRIPT}"
  RESULT_VARIABLE RESULT
  OUTPUT_VARIABLE OUTPUT
  ERROR_VARIABLE ERROR_OUTPUT
  TIMEOUT 20
)
if(NOT "${RESULT}" STREQUAL "1")
  message(FATAL_ERROR "Expected exit code 1, got [${RESULT}].\n${OUTPUT}\n${ERROR_OUTPUT}")
endif()
string(FIND "${OUTPUT}${ERROR_OUTPUT}" "game exited before script finished" FOUND)
if(FOUND EQUAL -1)
  message(FATAL_ERROR "Missing unexpected-exit diagnostic.\n${OUTPUT}\n${ERROR_OUTPUT}")
endif()
