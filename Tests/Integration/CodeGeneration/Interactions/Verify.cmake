function(run_step name)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(WRITE "${BINARY_DIR}/${name}.log" "${output}${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Generated interactions ${TARGET_VERSION}: ${name} failed (${result}). See ${BINARY_DIR}/${name}.log\n${output}${error}")
    endif()
endfunction()

foreach(required COMPILER TARGET_VERSION LVGL_SOURCE_DIR LVGL_BUILD_DIR BINARY_DIR SOURCE_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Set ${required} to run the generated interaction checks")
    endif()
endforeach()
file(MAKE_DIRECTORY "${BINARY_DIR}")
find_library(actions_lvgl NAMES lvgl
    PATHS "${LVGL_BUILD_DIR}" "${LVGL_BUILD_DIR}/Release" NO_DEFAULT_PATH)
if(NOT actions_lvgl)
    message(FATAL_ERROR "Run LVGLGenerator for ${TARGET_VERSION} first to prepare its LVGL archive in ${LVGL_BUILD_DIR}")
endif()

run_step(generate "${COMPILER}"
    "${SOURCE_ROOT}/Tests/Integration/CodeGeneration/Interactions.json"
    --target "${TARGET_VERSION}" --format c --output "${BINARY_DIR}/generated")
find_program(actions_ninja ninja)
if(actions_ninja)
    set(generator Ninja)
elseif(PARENT_GENERATOR)
    set(generator "${PARENT_GENERATOR}")
else()
    message(FATAL_ERROR "Ninja was not found; set PARENT_GENERATOR")
endif()
run_step(configure "${CMAKE_COMMAND}"
    -S "${SOURCE_ROOT}/Tests/Integration/CodeGeneration/Interactions" -B "${BINARY_DIR}"
    -G "${generator}" -DCMAKE_BUILD_TYPE=Release
    "-DLVGL_ARCHIVE=${actions_lvgl}" "-DLVGL_SOURCE_DIR=${LVGL_SOURCE_DIR}"
    "-DGENERATED_DIR=${BINARY_DIR}/generated")
run_step(build "${CMAKE_COMMAND}" --build "${BINARY_DIR}" --config Release --parallel 3)
run_step(tests "${CMAKE_CTEST_COMMAND}" --test-dir "${BINARY_DIR}" -C Release --output-on-failure)
