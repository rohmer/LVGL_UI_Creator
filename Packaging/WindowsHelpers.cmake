# Only project creation requires a target-native helper runtime.
# Font and image conversion are linked native C++ libraries.
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    return()
endif()
find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
set(UICREATOR_WINDOWS_X86_HELPER_ROOT "${PROJECT_SOURCE_DIR}/build/windows-helper-tools/x86" CACHE PATH
    "Private checksum-verified Windows x86 project-helper Python")
message(STATUS "Preparing native x86 Windows project helper")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Packaging/BuildWindowsHelpers.py"
    --architecture x86 --output "${UICREATOR_WINDOWS_X86_HELPER_ROOT}"
    RESULT_VARIABLE helper_result)
if(NOT helper_result EQUAL 0)
    message(FATAL_ERROR "Native x86 helper preparation failed; inspect ${UICREATOR_WINDOWS_X86_HELPER_ROOT}/helper-build.log")
endif()
file(READ "${UICREATOR_WINDOWS_X86_HELPER_ROOT}/cmake-helpers.json" helpers)
foreach(pair IN ITEMS "projectPython|UICREATOR_PROJECT_PYTHON|FILEPATH"
    "projectToolDirectory|UICREATOR_PREBUILT_PROJECT_TOOL_DIR|PATH")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 property)
    list(GET fields 1 variable)
    list(GET fields 2 type)
    string(JSON path GET "${helpers}" "${property}")
    file(TO_CMAKE_PATH "${path}" path)
    set(${variable} "${path}" CACHE ${type} "Verified target-native x86 helper" FORCE)
endforeach()
set(UICREATOR_WINDOWS_HELPERS_X86 TRUE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/Packaging/BuildWindowsHelpers.py"
    "${PROJECT_SOURCE_DIR}/UICreator/ProjectCreator/project_creator.py"
    "${PROJECT_SOURCE_DIR}/UICreator/ProjectCreator/raspberry_pi_linux.py"
    "${PROJECT_SOURCE_DIR}/UICreator/ProjectCreator/raspberry_pi_pico.py")
