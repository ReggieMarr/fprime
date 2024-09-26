include_guard()
include(utilities)

function(plantuml_sm_add_global_target TARGET)
    message(STATUS "[plantuml_sm] Adding global target: ${TARGET}")
    add_custom_target(${TARGET})
    set_property(TARGET ${TARGET} PROPERTY GLOBAL_PLANTUML_FILES "")
endfunction()

function(plantuml_sm_add_deployment_target MODULE TARGET SOURCES DEPENDENCIES FULL_DEPENDENCIES)
    message(STATUS "[plantuml_sm] Adding deployment target: ${TARGET} for module ${MODULE}")
    plantuml_sm_add_module_target("${MODULE}" "${TARGET}" "${SOURCES}" "${DEPENDENCIES}")
endfunction()

function(plantuml_sm_add_module_target MODULE TARGET SOURCES DEPENDENCIES)
    message(STATUS "[plantuml_sm] Adding module target: ${TARGET} for module ${MODULE}")
    foreach(SOURCE IN LISTS SOURCES)
        if(SOURCE MATCHES ".*\\.plantuml$")
            message(STATUS "[plantuml_sm]  Found PlantUML file: ${SOURCE}")
            get_filename_component(PLANTUML_DIR ${SOURCE} DIRECTORY)
            get_filename_component(PLANTUML_FILENAME ${SOURCE} NAME_WE)

            # Determine the relative path from the source root to the PlantUML file
            file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}" "${PLANTUML_DIR}")

            # Construct the output directory in the build tree
            set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/${REL_PATH}")
            message(STATUS "[plantuml_sm]  Sending to ${OUTPUT_DIR}")

            # TODO add these to sources automatically and put them in the build dir
            set(OUTPUT_FILES
                "${OUTPUT_DIR}/${PLANTUML_FILENAME}.cpp"
                "${OUTPUT_DIR}/${PLANTUML_FILENAME}.hpp"
                "${OUTPUT_DIR}/sendEvent.cpp"
                "${OUTPUT_DIR}/sendEvent.hpp"
                "${OUTPUT_DIR}/SignalGen.cpp"
                "${OUTPUT_DIR}/SignalGen.hpp"
            )

            # Ensure the output directory exists
            file(MAKE_DIRECTORY "${OUTPUT_DIR}")

            add_custom_command(
                OUTPUT ${OUTPUT_FILES}
                COMMAND ${PYTHON_EXECUTABLE} "${FPRIME_FRAMEWORK_PATH}/STARS/autocoder/Stars.py"
                        -backend fprime
                        -namespace ${PROJECT_NAME}
                        -model "${SOURCE}"
                DEPENDS "${SOURCE}"
                WORKING_DIRECTORY ${PLANTUML_DIR}
                COMMENT "Generating files for ${PLANTUML_FILENAME} using ${FPRIME_FRAMEWORK_PATH}/STARS/autocoder/Stars.py"
            )

            add_custom_target(${MODULE}_${PLANTUML_FILENAME}_generate
                DEPENDS ${OUTPUT_FILES}
            )

            add_dependencies(${TARGET} ${MODULE}_${PLANTUML_FILENAME}_generate)

            set_property(
                TARGET ${TARGET}
                APPEND PROPERTY GLOBAL_PLANTUML_FILES "${SOURCE}"
            )
        endif()
    endforeach()
endfunction()
