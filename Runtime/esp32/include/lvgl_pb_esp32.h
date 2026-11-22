#ifndef LVGL_PB_ESP32_H
#define LVGL_PB_ESP32_H
#include "lvgl_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char* name;
    const uint8_t* data;
    size_t size;
} lvgl_pb_embedded_project_t;
extern const lvgl_pb_embedded_project_t lvgl_pb_embedded_projects[];
extern const size_t lvgl_pb_embedded_project_count;

/* Install an esp_timer monotonic clock callback after lv_init(). No timer task
 * is allocated. Omit this if the application's LVGL port already owns ticks. */
void lvgl_pb_esp32_configure_tick(void);
/* Flash input is read directly by the loader. Its resources are owned by the
 * returned runtime. Call only from the LVGL task with the display alive. */
bool lvgl_pb_esp32_load_embedded(const char* name, const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output, char* error, size_t error_size);
/* ESP-IDF VFS path, e.g. /spiffs/dashboard.pb or /sdcard/project.xml. XML needs
 * the optional codec enabled; the same widget profile applies. Mount the
 * filesystem in application code first. One temporary bounded input buffer is
 * released before this call returns. This is distinct from LVGL drive paths. */
bool lvgl_pb_esp32_load_file(const char* path, const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output, char* error, size_t error_size);
#ifdef __cplusplus
}
#endif
#endif
