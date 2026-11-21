#include "lvgl_pb_board.h"
#include <stdio.h>
#include <stdlib.h>
static void flush(lvgl_pb_board_t* board, int32_t x1, int32_t y1, int32_t x2, int32_t y2, const void* pixels, size_t bytes, void* context) {
    (void)x1; (void)y1; (void)x2; (void)y2; (void)pixels; (void)bytes; (void)context;
    /* Copy/transfer these pixels to the real display here. For asynchronous
     * hardware, call flush_ready only when that transfer has completed. */
    lvgl_pb_board_flush_ready(board);
}
int main(int argc, char** argv) {
    if (argc != 2) { fprintf(stderr, "Usage: %s project.pb\n", argv[0]); return 2; }
    FILE* file = fopen(argv[1], "rb"); if (!file) return 2;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return 2; }
    long size = ftell(file); if (size < 1 || size > 16 * 1024 * 1024 || fseek(file, 0, SEEK_SET)) { fclose(file); return 2; }
    unsigned char* bytes = (unsigned char*)malloc((size_t)size);
    if (!bytes || fread(bytes, 1, (size_t)size, file) != (size_t)size) { free(bytes); fclose(file); return 2; }
    fclose(file); char error[512];
    lvgl_pb_board_t* board = lvgl_pb_board_create(640, 400, 0, flush, NULL, error, sizeof error);
    if (!board) { free(bytes); fprintf(stderr, "%s\n", error); return 1; }
    if (!lvgl_pb_board_load_memory(board, bytes, (size_t)size)) {
        fprintf(stderr, "%s\n", lvgl_pb_board_error(board)); free(bytes); lvgl_pb_board_destroy(board); return 1;
    }
    free(bytes);
    for (int frame = 0; frame < 20; ++frame) lvgl_pb_board_poll(board, 16);
    lvgl_pb_board_destroy(board); return 0;
}
