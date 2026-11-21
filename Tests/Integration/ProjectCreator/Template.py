from pathlib import Path
import sys,zipfile
out=Path(sys.argv[1]);out.parent.mkdir(parents=True,exist_ok=True)
cmake='''cmake_minimum_required(VERSION 3.24)
project(CreatedBoard LANGUAGES C CXX)
add_library(lvgl STATIC IMPORTED GLOBAL)
add_library(lvgl::lvgl ALIAS lvgl)
set_target_properties(lvgl PROPERTIES IMPORTED_LOCATION "${LVGL_ARCHIVE}" INTERFACE_INCLUDE_DIRECTORIES "${LVGL_SOURCE_DIR}" INTERFACE_COMPILE_DEFINITIONS "LV_CONF_SKIP=1;LV_COLOR_DEPTH=32;LV_COLOR_FORMAT_DEFAULT=LV_COLOR_FORMAT_ARGB8888;LV_USE_STDLIB_MALLOC=1;LV_MEM_CUSTOM=1;LV_GRADIENT_MAX_STOPS=8;LV_USE_CALENDAR_HEADER_ARROW=1;LV_USE_RLE=1;LV_USE_LZ4=1;LV_USE_LZ4_INTERNAL=1;LV_BIN_DECODER_RAM_LOAD=1;LV_USE_LODEPNG=1;LV_USE_TJPGD=1")
add_subdirectory(ui)
add_executable(board main.c)
target_link_libraries(board PRIVATE generated_ui)
enable_testing()
add_test(NAME CreatedBoardBoot COMMAND board)
'''
main='''#include "ui.h"
#include "lv_conf.h"
int main(void) {
    if(LV_USE_PERF_MONITOR != 1)return 1;
    lv_init();lv_display_t* display=lv_display_create(320,240);
    ui_init();
    if(!ui_get_screen(0) || !ui_get_object(0,0))return 2;
    ui_deinit();lv_display_delete(display);lv_deinit();return 0;
}
'''
with zipfile.ZipFile(out,'w') as z:
    for name,text in {'CMakeLists.txt':cmake,'main.c':main,'lv_conf.h':'#define LV_USE_PERF_MONITOR 0\n','ui/.gitkeep':''}.items():
        z.writestr('template/'+name,text)
