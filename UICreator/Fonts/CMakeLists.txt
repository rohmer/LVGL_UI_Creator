find_program(UICREATOR_FONT_NODE NAMES node
    HINTS "${PROJECT_SOURCE_DIR}/build/font-tools/node-v24.11.1-win-x64"
    DOC "Node runtime bundled with the LVGL font converter")
if(WIN32)
    find_program(UICREATOR_FONT_NPM NAMES npm.cmd npm)
else()
    find_program(UICREATOR_FONT_NPM NAMES npm)
endif()
if(NOT UICREATOR_FONT_NODE OR NOT UICREATOR_FONT_NPM)
    message(FATAL_ERROR "Node.js and npm are required to build the font converter. They are bundled with the application after building.")
endif()
set(font_converter_source "${PROJECT_SOURCE_DIR}/3rdParty/lv_font_conv")
if(NOT EXISTS "${font_converter_source}/node_modules/argparse/package.json")
    execute_process(COMMAND "${UICREATOR_FONT_NPM}" ci --omit=dev --ignore-scripts --no-audit --no-fund
        WORKING_DIRECTORY "${font_converter_source}" RESULT_VARIABLE font_install_result)
    if(NOT font_install_result EQUAL 0)
        message(FATAL_ERROR "Could not install the pinned font converter dependencies")
    endif()
endif()
get_filename_component(font_node_directory "${UICREATOR_FONT_NODE}" DIRECTORY)
get_filename_component(font_node_name "${UICREATOR_FONT_NODE}" NAME)
add_custom_target(UICreatorFontConverterAssets
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/font-converter"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${font_converter_source}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/font-converter"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${UICREATOR_FONT_NODE}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/font-converter/${font_node_name}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/font-converter-bridge.js" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/font-converter/bridge.js"
    VERBATIM)
if(EXISTS "${font_node_directory}/LICENSE")
    add_custom_command(TARGET UICreatorFontConverterAssets POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${font_node_directory}/LICENSE" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/font-converter/NODE_LICENSE")
endif()
add_library(UICreatorFontConversion STATIC FontConverter.cpp)
if(APPLE)
    target_link_libraries(UICreatorFontConversion PRIVATE SDL3::SDL3)
endif()
target_include_directories(UICreatorFontConversion PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(UICreatorFontConversion PRIVATE /utf-8)
endif()
if(BUILD_TESTING)
    add_executable(UICreatorFontConverterTests "${PROJECT_SOURCE_DIR}/Tests/Integration/FontConverter.cpp")
    target_link_libraries(UICreatorFontConverterTests PRIVATE UICreatorFontConversion LVSerial)
    add_dependencies(UICreatorFontConverterTests UICreatorFontConverterAssets)
    add_test(NAME UICreatorFontConverter COMMAND UICreatorFontConverterTests "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-output")
    set_tests_properties(UICreatorFontConverter PROPERTIES TIMEOUT 120)
    set(font_fixture "${CMAKE_CURRENT_BINARY_DIR}/compiled-font")
    add_custom_command(OUTPUT "${font_fixture}/converted_font.c" "${font_fixture}/converted_font.h"
        COMMAND UICreatorFontConverterTests --fixture "${PROJECT_SOURCE_DIR}" "${font_fixture}"
        DEPENDS UICreatorFontConverterTests VERBATIM)
    add_executable(UICreatorConvertedFontTests "${PROJECT_SOURCE_DIR}/Tests/Integration/FontGenerated.c" "${font_fixture}/converted_font.c")
    target_include_directories(UICreatorConvertedFontTests PRIVATE "${font_fixture}")
    target_link_libraries(UICreatorConvertedFontTests PRIVATE lvgl::lvgl)
    set_target_properties(UICreatorConvertedFontTests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
    add_test(NAME UICreatorConvertedFontC COMMAND UICreatorConvertedFontTests)
endif()
