# Runs Scenic with a script against a throwaway copy of a fixture project, so tests never edit
# authored files and always start from the same state.
file(REMOVE_RECURSE ${PROJECT})
file(COPY ${FIXTURE}/ DESTINATION ${PROJECT})

# Session files stay out of the user's home directory. Socket paths have a short length limit, so
# they cannot live in the build tree.
set(ENV{SCENIC_HOME} /tmp/scenic-editor-tests)
execute_process(
  COMMAND ${EDITOR} --project ${PROJECT} --script ${SCRIPT} --log ${PROJECT}/editor.log
  RESULT_VARIABLE RESULT
)
if(NOT RESULT EQUAL 0)
  message(FATAL_ERROR "Editor script failed with exit code ${RESULT}; see ${PROJECT}/editor.log")
endif()
