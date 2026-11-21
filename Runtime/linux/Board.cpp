#include "lvgl_pb_board.h"
#include "lvgl_runtime.h"
#include <lvgl.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct lvgl_pb_board {
    lv_display_t* display = nullptr;
    lv_indev_t* pointer = nullptr;
    lvgl_runtime_t* runtime = nullptr;
    lvgl_pb_board_flush_t flush = nullptr;
    void* context = nullptr;
    lv_point_t point{};
    bool pressed = false;
    std::vector<uint8_t> buffer;
    std::string error;
};
namespace {
void Error(char* output, size_t capacity, const char* message) {
    if (output && capacity) { const auto count = (std::min)(capacity - 1, std::strlen(message)); std::memcpy(output, message, count); output[count] = 0; }
}
void Flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    auto* board = static_cast<lvgl_pb_board*>(lv_display_get_user_data(display));
    const auto stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), lv_display_get_color_format(display));
    board->flush(board, area->x1, area->y1, area->x2, area->y2, pixels,
        size_t(stride) * lv_area_get_height(area), board->context);
}
void Read(lv_indev_t* input, lv_indev_data_t* data) {
    const auto* board = static_cast<lvgl_pb_board*>(lv_indev_get_user_data(input));
    data->point = board->point; data->state = board->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
bool Load(lvgl_pb_board* board, const void* bytes, size_t size, bool borrowed) {
    if (!board) return false;
    lvgl_runtime_t* candidate = nullptr; char error[512]{};
    const lvgl_runtime_options_t options{board->display, 16u * 1024u * 1024u, true};
    const bool loaded = borrowed ? lvgl_runtime_load_memory_borrowed_assets(bytes, size, &options, &candidate, error, sizeof error)
                                 : lvgl_runtime_load_memory(bytes, size, &options, &candidate, error, sizeof error);
    if (!loaded) { board->error = error; return false; }
    lvgl_runtime_destroy(board->runtime); board->runtime = candidate; board->error.clear(); return true;
}
}
extern "C" {
lvgl_pb_board_t* lvgl_pb_board_create(int32_t width, int32_t height, unsigned depth,
    lvgl_pb_board_flush_t flush, void* context, char* error, size_t capacity) {
    if (!flush || width < 1 || height < 1 || width > 8192 || height > 8192 || (depth && depth != 16 && depth != 32)) {
        Error(error, capacity, "Invalid board dimensions, color depth or flush callback"); return nullptr;
    }
    if (lv_is_initialized()) { Error(error, capacity, "An LVGL display owner already exists"); return nullptr; }
    try {
        auto board = std::make_unique<lvgl_pb_board>(); board->flush = flush; board->context = context;
        lv_init(); board->display = lv_display_create(width, height);
        if (!board->display) throw std::bad_alloc();
        if (depth) lv_display_set_color_format(board->display, depth == 16 ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_ARGB8888);
        const auto stride = lv_draw_buf_width_to_stride(width, lv_display_get_color_format(board->display));
        board->buffer.resize(size_t(stride) * (std::min)(height, 16));
        lv_display_set_buffers(board->display, board->buffer.data(), nullptr, static_cast<uint32_t>(board->buffer.size()), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_user_data(board->display, board.get()); lv_display_set_flush_cb(board->display, Flush);
        board->pointer = lv_indev_create(); if (!board->pointer) throw std::bad_alloc();
        lv_indev_set_type(board->pointer, LV_INDEV_TYPE_POINTER); lv_indev_set_display(board->pointer, board->display);
        lv_indev_set_user_data(board->pointer, board.get()); lv_indev_set_read_cb(board->pointer, Read);
        Error(error, capacity, ""); return board.release();
    } catch (const std::exception& failure) { if (lv_is_initialized()) lv_deinit(); Error(error, capacity, failure.what()); return nullptr; }
}
void lvgl_pb_board_destroy(lvgl_pb_board_t* board) { if (board) { lvgl_runtime_destroy(board->runtime); lv_deinit(); delete board; } }
void lvgl_pb_board_flush_ready(lvgl_pb_board_t* board) { if (board && board->display) lv_display_flush_ready(board->display); }
void lvgl_pb_board_poll(lvgl_pb_board_t* board, uint32_t elapsed) { if (board) { lv_tick_inc(elapsed); lv_timer_handler(); } }
void lvgl_pb_board_pointer(lvgl_pb_board_t* board, int32_t x, int32_t y, bool pressed) {
    if (board) { board->point = {x, y}; board->pressed = pressed; lv_indev_read(board->pointer); }
}
bool lvgl_pb_board_load_memory(lvgl_pb_board_t* board, const void* bytes, size_t size) { return Load(board, bytes, size, false); }
bool lvgl_pb_board_load_memory_borrowed_assets(lvgl_pb_board_t* board, const void* bytes, size_t size) { return Load(board, bytes, size, true); }
const char* lvgl_pb_board_error(const lvgl_pb_board_t* board) { return board ? board->error.c_str() : "No board display"; }
void* lvgl_pb_board_object(lvgl_pb_board_t* board, size_t screen, int32_t id) { return board ? lvgl_runtime_object(board->runtime, screen, id) : nullptr; }
bool lvgl_pb_board_get_variable(lvgl_pb_board_t* board, const char* name, lvgl_pb_value_t* value) {
    if (!board || !value) return false; lvgl_runtime_value_t native{};
    if (!lvgl_runtime_get_variable(board->runtime, name, &native)) return false;
    *value = {static_cast<lvgl_pb_value_type_t>(native.type), native.string_value, native.integer_value, native.real_value, native.boolean_value}; return true;
}
bool lvgl_pb_board_set_variable(lvgl_pb_board_t* board, const char* name, const lvgl_pb_value_t* value) {
    if (!board || !value || value->type < LVGL_PB_STRING || value->type > LVGL_PB_DATETIME) return false;
    const lvgl_runtime_value_t native{static_cast<lvgl_runtime_value_type_t>(value->type), value->string_value, value->integer_value, value->real_value, value->boolean_value};
    return lvgl_runtime_set_variable(board->runtime, name, &native);
}
}
