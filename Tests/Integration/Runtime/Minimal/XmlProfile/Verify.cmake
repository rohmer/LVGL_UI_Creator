cmake_minimum_required(VERSION 3.24)
foreach(required SOURCE_ROOT BINARY_DIR PYTHON)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
file(MAKE_DIRECTORY "${BINARY_DIR}")
function(run_step name)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    file(WRITE "${BINARY_DIR}/${name}.log" "${output}\n${errors}")
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "XML minimal profile ${name} failed (${result})\n${output}\n${errors}")
    endif()
endfunction()
run_step(bundle "${PYTHON}" "${SOURCE_ROOT}/Runtime/esp32/make_component.py"
    "${SOURCE_ROOT}/Tests/Integration/Runtime/Minimal/label.json" --output "${BINARY_DIR}/component" --replace)
if(NOT PARENT_GENERATOR)
    set(PARENT_GENERATOR Ninja)
endif()
foreach(mode ON OFF)
    run_step(configure-${mode} "${CMAKE_COMMAND}"
        -S "${SOURCE_ROOT}/Tests/Integration/Runtime/Minimal/XmlProfile" -B "${BINARY_DIR}/build-${mode}"
        -G "${PARENT_GENERATOR}" -DCMAKE_BUILD_TYPE=Release
        "-DLVGL_PB_COMPONENT_DIR=${BINARY_DIR}/component" "-DLVGL_PB_ENABLE_XML=${mode}"
        "-DXML_FIXTURE=${BINARY_DIR}/fixture.xml")
    run_step(build-${mode} "${CMAKE_COMMAND}" --build "${BINARY_DIR}/build-${mode}" --config Release --parallel 4)
    run_step(test-${mode} "${CMAKE_CTEST_COMMAND}" --test-dir "${BINARY_DIR}/build-${mode}" -C Release --output-on-failure)
endforeach()
message(STATUS "One minimal widget profile compiled and passed with its XML codec enabled and disabled")
