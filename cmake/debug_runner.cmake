# debug_runner.cmake

message("Running Stars.py with the following arguments:")
message("PYTHON_EXECUTABLE: ${PYTHON_EXECUTABLE}")
message("STARS_SCRIPT: ${STARS_SCRIPT}")
message("BACKEND: ${BACKEND}")
message("NAMESPACE: ${NAMESPACE}")
message("INPUT_FILES: ${INPUT_FILES}")

execute_process(
    COMMAND ${PYTHON_EXECUTABLE} ${STARS_SCRIPT}
            -backend ${BACKEND}
            -namespace ${NAMESPACE}
            -model ${INPUT_FILES}
    RESULT_VARIABLE STARS_RESULT
    OUTPUT_VARIABLE STARS_OUTPUT
    ERROR_VARIABLE STARS_ERROR
)

message("Stars.py execution result: ${STARS_RESULT}")
message("Stars.py stdout: ${STARS_OUTPUT}")
message("Stars.py stderr: ${STARS_ERROR}")

if(NOT STARS_RESULT EQUAL 0)
    message("Stars.py execution result: ${STARS_RESULT}")
    message("Stars.py stdout: ${STARS_OUTPUT}")
    message("Stars.py stderr: ${STARS_ERROR}")
endif()
