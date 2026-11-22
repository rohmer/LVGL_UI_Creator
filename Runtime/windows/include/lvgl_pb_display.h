#ifndef LVGL_PB_DISPLAY_H
#define LVGL_PB_DISPLAY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32) && defined(LVGL_PB_DISPLAY_BUILD)
#define LVGL_PB_API __declspec(dllexport)
#elif defined(_WIN32)
#define LVGL_PB_API __declspec(dllimport)
#else
#define LVGL_PB_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lvgl_pb_window lvgl_pb_window_t;
typedef enum {
    LVGL_PB_STRING, LVGL_PB_INTEGER, LVGL_PB_BOOLEAN, LVGL_PB_FLOAT, LVGL_PB_DATETIME
} lvgl_pb_value_type_t;
typedef struct {
    lvgl_pb_value_type_t type;
    const char* string_value;
    int64_t integer_value;
    double real_value;
    bool boolean_value;
} lvgl_pb_value_t;
/* One owner window per process; call every API on the creating UI thread.
 * The DLL owns its LVGL instance and SDL window, independently of the editor.
 * UTF-8 strings and filenames. No caller-side LVGL installation is required. */
LVGL_PB_API lvgl_pb_window_t* lvgl_pb_window_create(int width, int height, const char* title, char* error, size_t error_size);
LVGL_PB_API void lvgl_pb_window_destroy(lvgl_pb_window_t* window);
/* Accepts Protobuf and LVGL UI Creator XML when its codec is enabled.
 * Failed loads preserve the previously displayed project. Input is consumed
 * during this call; retained models/resources are owned by the DLL. The caller
 * may release bytes after this returns. Input limit is 16 MiB. */
LVGL_PB_API bool lvgl_pb_window_load_memory(lvgl_pb_window_t* window, const void* bytes, size_t size);
LVGL_PB_API bool lvgl_pb_window_load_file(lvgl_pb_window_t* window, const char* utf8_path);
/* Pump regularly until false; honors the native window close button. */
LVGL_PB_API bool lvgl_pb_window_poll(lvgl_pb_window_t* window);
LVGL_PB_API const char* lvgl_pb_window_error(const lvgl_pb_window_t* window);
LVGL_PB_API size_t lvgl_pb_window_screen_count(const lvgl_pb_window_t* window);
/* Returns the currently displayed project screen index. False for invalid
 * arguments, an unloaded window, or a display without a project screen active. */
LVGL_PB_API bool lvgl_pb_window_active_screen(const lvgl_pb_window_t* window, size_t* screen);
LVGL_PB_API bool lvgl_pb_window_show_screen(lvgl_pb_window_t* window, size_t screen);
/* Advanced inspection: borrowed native LVGL 9.6 object, invalid after reload. */
LVGL_PB_API void* lvgl_pb_window_object(lvgl_pb_window_t* window, size_t screen, int32_t object_id);
LVGL_PB_API size_t lvgl_pb_window_variable_count(const lvgl_pb_window_t* window);
LVGL_PB_API const char* lvgl_pb_window_variable_name(const lvgl_pb_window_t* window, size_t index);
LVGL_PB_API bool lvgl_pb_window_get_variable(lvgl_pb_window_t* window, const char* name, lvgl_pb_value_t* value);
LVGL_PB_API bool lvgl_pb_window_set_variable(lvgl_pb_window_t* window, const char* name, const lvgl_pb_value_t* value);
LVGL_PB_API bool lvgl_pb_window_save_bmp(lvgl_pb_window_t* window, const char* utf8_path);
/* Keyboard viewer helpers; applications can use the functions above directly. */
LVGL_PB_API void lvgl_pb_window_set_title(lvgl_pb_window_t* window, const char* title);
#ifdef __cplusplus
}
#endif
#endif
