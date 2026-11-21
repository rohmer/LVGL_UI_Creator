#include "lvgl_pb_board.h"
#include <stdio.h>
static unsigned flushes;
static void flush(lvgl_pb_board_t* board, int32_t x1, int32_t y1, int32_t x2, int32_t y2, const void* pixels, size_t bytes, void* context) {
    (void)x1; (void)y1; (void)x2; (void)y2; (void)context;
    if (pixels && bytes) ++flushes;
    lvgl_pb_board_flush_ready(board);
}
int main(void) {
    char error[256];
    if (lvgl_pb_board_create(0, 240, 32, flush, NULL, error, sizeof error)) return 1;
    for (int n = 0; n < 3; ++n) {
        lvgl_pb_board_t* board = lvgl_pb_board_create(320, 240, 0, flush, NULL, error, sizeof error);
        if (!board) { fprintf(stderr, "%s\n", error); return 2; }
        const unsigned char invalid[] = {255, 255};
        if (lvgl_pb_board_load_memory(board, invalid, sizeof invalid)) return 3;
        lvgl_pb_board_poll(board, 32); lvgl_pb_board_pointer(board, 10, 10, true); lvgl_pb_board_pointer(board, 10, 10, false);
        lvgl_pb_board_destroy(board);
    }
    if (!flushes) return 4;
    puts("Linux board callback render/input/lifecycle passed"); return 0;
}
