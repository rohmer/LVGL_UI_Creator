add_library(UICreatorFontConversion STATIC FontConverter.cpp NativeFont.cpp NativeFontBinary.cpp)
target_link_libraries(UICreatorFontConversion PRIVATE freetype UICreatorNativePng)
target_include_directories(UICreatorFontConversion PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(UICreatorFontConversion PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(UICreatorFontConversion PRIVATE /utf-8)
endif()
if(BUILD_TESTING)
    add_executable(UICreatorFontConverterTests "${PROJECT_SOURCE_DIR}/Tests/Integration/FontConverter.cpp")
    target_link_libraries(UICreatorFontConverterTests PRIVATE UICreatorFontConversion LVSerial freetype)
    add_test(NAME UICreatorFontConverter COMMAND UICreatorFontConverterTests "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-output")
    set_tests_properties(UICreatorFontConverter PROPERTIES TIMEOUT 120)
    set(font_fixture "${CMAKE_CURRENT_BINARY_DIR}/compiled-font")
    add_custom_command(OUTPUT "${font_fixture}/converted_font.c" "${font_fixture}/converted_font.h"
        "${font_fixture}/converted_kern_font.c" "${font_fixture}/converted_kern_font.h"
        COMMAND UICreatorFontConverterTests --fixture "${PROJECT_SOURCE_DIR}" "${font_fixture}"
        DEPENDS UICreatorFontConverterTests VERBATIM)
    add_executable(UICreatorConvertedFontTests "${PROJECT_SOURCE_DIR}/Tests/Integration/FontGenerated.c"
        "${font_fixture}/converted_font.c" "${font_fixture}/converted_kern_font.c")
    target_include_directories(UICreatorConvertedFontTests PRIVATE "${font_fixture}")
    target_link_libraries(UICreatorConvertedFontTests PRIVATE lvgl::lvgl)
    set_target_properties(UICreatorConvertedFontTests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
    add_test(NAME UICreatorConvertedFontC COMMAND UICreatorConvertedFontTests)
endif()
