#include "lvgl_pb_board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned frames;
static void flush(lvgl_pb_board_t* board, int32_t x1, int32_t y1, int32_t x2, int32_t y2, const void* pixels, size_t bytes, void* context) {
    (void)x1; (void)y1; (void)x2; (void)y2; (void)context;
    if (pixels && bytes) ++frames;
    lvgl_pb_board_flush_ready(board);
}
static void click(lvgl_pb_board_t* board, int x, int y) {
    lvgl_pb_board_pointer(board, x, y, false);
    lvgl_pb_board_pointer(board, x, y, true);
    lvgl_pb_board_pointer(board, x, y, false);
}
static unsigned char* read_file(const char* path, size_t* size) {
    FILE* file = fopen(path, "rb"); if (!file) return NULL;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return NULL; }
    long count = ftell(file); if (count < 1 || count > 16 * 1024 * 1024 || fseek(file, 0, SEEK_SET)) { fclose(file); return NULL; }
    unsigned char* bytes = (unsigned char*)malloc((size_t)count);
    if (!bytes || fread(bytes, 1, (size_t)count, file) != (size_t)count) { free(bytes); fclose(file); return NULL; }
    fclose(file); *size = (size_t)count; return bytes;
}
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    for (unsigned borrowed = 0; borrowed <= 1; ++borrowed) for (int lifetime = 0; lifetime < 3; ++lifetime) {
        size_t size = 0; unsigned char* bytes = read_file(argv[1], &size); if (!bytes) return 2;
        char error[512]; lvgl_pb_board_t* board = lvgl_pb_board_create(640, 400, 0, flush, NULL, error, sizeof error);
        if (!board) { fprintf(stderr, "%s\n", error); return 3; }
        const bool loaded = borrowed ? lvgl_pb_board_load_memory_borrowed_assets(board, bytes, size)
                                     : lvgl_pb_board_load_memory(board, bytes, size);
        if (!loaded) { fprintf(stderr, "%s\n", lvgl_pb_board_error(board)); return 4; }
        if (!borrowed) { free(bytes); bytes = NULL; }
        if (!lvgl_pb_board_object(board, 0, 7) || !lvgl_pb_board_object(board, 0, 8)) return 5;
        lvgl_pb_value_t value = {0}; value.type = LVGL_PB_STRING; value.string_value = "";
        if (!lvgl_pb_board_set_variable(board, "message", &value)) return 6;
        lvgl_pb_board_poll(board, 40); click(board, 30, 30);
        click(board, 280, 190); /* t */
        click(board, 160, 190); /* e */
        click(board, 370, 238); /* h */
        if (!lvgl_pb_board_get_variable(board, "message", &value) || strcmp(value.string_value, "teh")) {
            fprintf(stderr, "External typing produced: %s\n", value.string_value ? value.string_value : "(null)"); return 7;
        }
        click(board, 310, 335); /* space commits actual trie correction */
        if (!lvgl_pb_board_get_variable(board, "message", &value) || strcmp(value.string_value, "the ")) {
            fprintf(stderr, "Autocorrect produced: %s\n", value.string_value ? value.string_value : "(null)"); return 8;
        }
        value.string_value = "teh";
        if (!lvgl_pb_board_set_variable(board, "message", &value)) return 12;
        click(board, 550, 335); /* Done/Enter commits and closes, without newline. */
        if (!lvgl_pb_board_get_variable(board, "message", &value) || strcmp(value.string_value, "the")) return 13;
        click(board, 280, 190);
        if (!lvgl_pb_board_get_variable(board, "message", &value) || strcmp(value.string_value, "the")) return 14;
        value.string_value = ""; if (!lvgl_pb_board_set_variable(board, "message", &value)) return 15;
        click(board, 30, 30); click(board, 280, 190);
        if (!lvgl_pb_board_get_variable(board, "message", &value) || strcmp(value.string_value, "t")) return 16;
        click(board, 587, 143); /* X in the reserved right end of the suggestion bar. */
        click(board, 370, 238);
        if (!lvgl_pb_board_get_variable(board, "message", &value) || strcmp(value.string_value, "t")) return 17;
        lvgl_pb_board_poll(board, 80);
        const unsigned char invalid[] = {255, 255};
        if (lvgl_pb_board_load_memory(board, invalid, sizeof invalid)) return 9;
        if (!lvgl_pb_board_get_variable(board, "message", &value) || strcmp(value.string_value, "t")) return 10;
        lvgl_pb_board_destroy(board); free(bytes);
    }
    if (!frames) return 11;
    puts("Linux external keyboard, real trie correction, owned/borrowed binary assets and six lifetimes passed"); return 0;
}
