# The 32-bit desktop must also launch genuine 32-bit converter processes.
# Keep the existing Linux and x64 dependency/build paths unchanged.
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    return()
endif()
find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
set(UICREATOR_WINDOWS_X86_HELPER_ROOT "${PROJECT_SOURCE_DIR}/build/windows-helper-tools/x86" CACHE PATH
    "Private checksum-verified Windows x86 Node/Python converter tools")
message(STATUS "Preparing native x86 Windows converter helpers")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Packaging/BuildWindowsHelpers.py"
    --architecture x86 --output "${UICREATOR_WINDOWS_X86_HELPER_ROOT}"
    RESULT_VARIABLE helper_result)
if(NOT helper_result EQUAL 0)
    message(FATAL_ERROR "Native x86 helper preparation failed; inspect ${UICREATOR_WINDOWS_X86_HELPER_ROOT}/helper-build.log")
endif()
file(READ "${UICREATOR_WINDOWS_X86_HELPER_ROOT}/cmake-helpers.json" helpers)
foreach(pair IN ITEMS "fontNode|UICREATOR_FONT_NODE|FILEPATH" "fontNpm|UICREATOR_FONT_NPM|FILEPATH"
    "imagePython|UICREATOR_IMAGE_PYTHON|FILEPATH" "imageToolDirectory|UICREATOR_PREBUILT_IMAGE_TOOL_DIR|PATH"
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
    "${PROJECT_SOURCE_DIR}/UICreator/Images/image-converter-bridge.py"
    "${PROJECT_SOURCE_DIR}/3rdParty/lvgl_image_converter/LVGLImage.py"
    "${PROJECT_SOURCE_DIR}/UICreator/ProjectCreator/project_creator.py"
    "${PROJECT_SOURCE_DIR}/UICreator/ProjectCreator/raspberry_pi_linux.py"
    "${PROJECT_SOURCE_DIR}/UICreator/ProjectCreator/raspberry_pi_pico.py")
