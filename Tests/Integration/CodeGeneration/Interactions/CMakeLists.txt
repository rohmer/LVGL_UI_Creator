cmake_minimum_required(VERSION 3.24)
project(GeneratedInteractionTests LANGUAGES C)

foreach(required LVGL_ARCHIVE LVGL_SOURCE_DIR GENERATED_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Set ${required} before configuring the generated interaction tests")
    endif()
endforeach()

# The regular generator matrix builds this archive with the same configuration.
# Reusing it keeps this test focused on the generated application's C callbacks.
add_library(lvgl STATIC IMPORTED GLOBAL)
set_target_properties(lvgl PROPERTIES
    IMPORTED_LOCATION "${LVGL_ARCHIVE}"
    INTERFACE_INCLUDE_DIRECTORIES "${LVGL_SOURCE_DIR}"
    INTERFACE_COMPILE_DEFINITIONS "LV_CONF_SKIP=1;LV_COLOR_DEPTH=32;LV_USE_STDLIB_MALLOC=1;LV_MEM_CUSTOM=1;LV_GRADIENT_MAX_STOPS=8;LV_USE_CALENDAR_HEADER_ARROW=1;LV_USE_RLE=1;LV_USE_LZ4=1;LV_USE_LZ4_INTERNAL=1;LV_BIN_DECODER_RAM_LOAD=1;LV_USE_LODEPNG=1;LV_USE_TJPGD=1")
if(EXISTS "${LVGL_SOURCE_DIR}/include/lvgl/config/lv_conf_internal.h")
    target_compile_definitions(lvgl INTERFACE LV_COLOR_FORMAT_DEFAULT=LV_COLOR_FORMAT_ARGB8888)
endif()
if(UNIX)
    target_link_libraries(lvgl INTERFACE m)
endif()

add_executable(generated_actions
    "${CMAKE_CURRENT_LIST_DIR}/../Interactions.c"
    "${GENERATED_DIR}/ui.c"
    "${GENERATED_DIR}/ui_assets.c")
target_link_libraries(generated_actions PRIVATE lvgl)
target_include_directories(generated_actions PRIVATE "${GENERATED_DIR}")
set_target_properties(generated_actions PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(generated_actions PRIVATE /W3 /we4013 /we4996 /utf-8)
else()
    target_compile_options(generated_actions PRIVATE -Werror=deprecated-declarations)
endif()

enable_testing()
add_test(NAME GeneratedActions COMMAND generated_actions)
set_tests_properties(GeneratedActions PROPERTIES TIMEOUT 30)
