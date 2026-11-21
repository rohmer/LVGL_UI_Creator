#include "lvgl_pb_display.h"
#include <stdio.h>
int main(int argc, char** argv) {
    if (argc != 3) return 1;
    for (int iteration = 0; iteration < 3; ++iteration) {
        char error[512]; size_t active = 99;
        lvgl_pb_window_t* window = lvgl_pb_window_create(320, 240, "Linux SDK test", error, sizeof error);
        if (!window) { fprintf(stderr, "%s\n", error); return 2; }
        if (!lvgl_pb_window_load_file(window, argv[1]) || !lvgl_pb_window_screen_count(window) || !lvgl_pb_window_active_screen(window, &active)) return 3;
        const unsigned char invalid[] = {255, 255};
        if (lvgl_pb_window_load_memory(window, invalid, sizeof invalid)) return 4;
        size_t preserved = 99;
        if (!lvgl_pb_window_active_screen(window, &preserved) || preserved != active) return 5;
        for (int frame = 0; frame < 4; ++frame) if (!lvgl_pb_window_poll(window)) return 6;
        if (!lvgl_pb_window_save_bmp(window, argv[2])) return 7;
        lvgl_pb_window_destroy(window);
    }
    puts("Linux shared C ABI load/render/rollback/lifecycle passed"); return 0;
}
