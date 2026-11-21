cmake_minimum_required(VERSION 3.24)
# The firmware API is C; the runtime implementation requires the C++ linker.
project(StandaloneRuntimeFirmware LANGUAGES C CXX)
add_library(lvgl STATIC IMPORTED GLOBAL)
set_target_properties(lvgl PROPERTIES IMPORTED_LOCATION "${LVGL_ARCHIVE}"
    INTERFACE_INCLUDE_DIRECTORIES "${LVGL_SOURCE_DIR}"
    INTERFACE_COMPILE_DEFINITIONS "LV_CONF_SKIP=1;LV_COLOR_DEPTH=32;LV_COLOR_FORMAT_DEFAULT=LV_COLOR_FORMAT_ARGB8888;LV_USE_STDLIB_MALLOC=1;LV_MEM_CUSTOM=1;LV_GRADIENT_MAX_STOPS=8;LV_USE_CALENDAR_HEADER_ARROW=1;LV_USE_RLE=1;LV_USE_LZ4=1;LV_USE_LZ4_INTERNAL=1;LV_BIN_DECODER_RAM_LOAD=1;LV_USE_LODEPNG=1;LV_USE_TJPGD=1")
set(BUILD_TESTING OFF)
add_subdirectory("${REPOSITORY_ROOT}/Runtime" runtime)
add_executable(firmware Main.c)
target_link_libraries(firmware PRIVATE LVGLRuntime::Runtime)
if(UNIX)
    target_link_libraries(firmware PRIVATE m)
endif()
enable_testing()
add_test(NAME StandaloneFirmware COMMAND firmware "${PROJECT_FILE}")
