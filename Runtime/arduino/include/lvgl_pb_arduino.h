#ifndef LVGL_PB_ARDUINO_H
#define LVGL_PB_ARDUINO_H
#include "lvgl_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {const char* name;const uint8_t* data;size_t size;} lvgl_pb_embedded_project_t;
extern const lvgl_pb_embedded_project_t lvgl_pb_embedded_projects[];
extern const size_t lvgl_pb_embedded_project_count;
/* Install Arduino millis() as the monotonic LVGL clock after lv_init(). */
void lvgl_pb_arduino_configure_tick(void);
/* Embedded arrays remain in flash for the returned runtime's lifetime. */
bool lvgl_pb_arduino_load_embedded(const char* name,const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output,char* error,size_t error_size);
#ifdef __cplusplus
}
#include <FS.h>
/* Mount the chosen filesystem in the sketch first (LittleFS, SD, etc.). This
 * bounded loader releases its temporary file buffer before returning. */
bool lvgl_pb_arduino_load_file(fs::FS& filesystem,const char* path,const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output,char* error,size_t error_size);
#endif
#endif
