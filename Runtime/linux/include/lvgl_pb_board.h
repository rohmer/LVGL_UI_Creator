#ifndef LVGL_PB_BOARD_H
#define LVGL_PB_BOARD_H
#include "lvgl_pb_display.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lvgl_pb_board lvgl_pb_board_t;
/* Called on the LVGL owner thread. Pixels are borrowed until flush_ready;
 * the callback must call lvgl_pb_board_flush_ready after consuming them.
 * Coordinates are inclusive and pixels use native RGB565 or ARGB8888. Rows
 * may include LVGL alignment padding: row stride is bytes / (y2-y1+1).
 * This adapter supplies no hardware driver; applications own framebuffer,
 * SPI, DRM or other transport setup. */
typedef void (*lvgl_pb_board_flush_t)(lvgl_pb_board_t* board, int32_t x1, int32_t y1,
    int32_t x2, int32_t y2, const void* pixels, size_t bytes, void* context);
/* One board/display owner per process; do not mix with window_create. Zero
 * color depth chooses the compiled profile's format; otherwise use 16 or 32.
 * A nonzero callback and 1..8192 dimensions are required. */
lvgl_pb_board_t* lvgl_pb_board_create(int32_t width, int32_t height, unsigned color_depth,
    lvgl_pb_board_flush_t flush, void* context, char* error, size_t error_size);
void lvgl_pb_board_destroy(lvgl_pb_board_t* board);
void lvgl_pb_board_flush_ready(lvgl_pb_board_t* board);
/* elapsed_ms advances LVGL time; call regularly on the creating thread. */
void lvgl_pb_board_poll(lvgl_pb_board_t* board, uint32_t elapsed_ms);
void lvgl_pb_board_pointer(lvgl_pb_board_t* board, int32_t x, int32_t y, bool pressed);
bool lvgl_pb_board_load_memory(lvgl_pb_board_t* board, const void* bytes, size_t size);
/* Protobuf immutable flash trie/index spans remain borrowed until destroy.
 * XML input (when enabled) always decodes resources into owned storage. */
bool lvgl_pb_board_load_memory_borrowed_assets(lvgl_pb_board_t* board, const void* bytes, size_t size);
const char* lvgl_pb_board_error(const lvgl_pb_board_t* board);
void* lvgl_pb_board_object(lvgl_pb_board_t* board, size_t screen, int32_t id);
bool lvgl_pb_board_get_variable(lvgl_pb_board_t* board, const char* name, lvgl_pb_value_t* value);
bool lvgl_pb_board_set_variable(lvgl_pb_board_t* board, const char* name, const lvgl_pb_value_t* value);
#ifdef __cplusplus
}
#endif
#endif
