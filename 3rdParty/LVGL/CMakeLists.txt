cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_C_STANDARD 11)

project(libLVGL C)

include_directories(.)
include_directories(lv_drivers)
include_directories(lv_drivers/display)
include_directories(lv_drivers/indev)
include_directories(lvgl)
include_directories(lvgl/src/lv_core)
include_directories(lvgl/src/lv_draw)
include_directories(lvgl/src/lv_fonts)
include_directories(lvgl/src/lv_hal)
include_directories(lvgl/src/lv_misc)
include_directories(lvgl/src/lv_objx)
include_directories(lvgl/src/lv_themes)
include_directories(../SDL2-2.0.10/include)

file(GLOB_RECURSE LVGL_SRC
    "*.h"
    "*.c"
)

add_library(LVGL SHARED
    ${LVGL_SRC}
)

link_directories(3rdParty/SDL2-2.0.10)
target_link_libraries(LVGL SDL2)