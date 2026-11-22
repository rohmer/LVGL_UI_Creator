#ifndef LVGL_PB_PICO_H
#define LVGL_PB_PICO_H
#include "lvgl_runtime.h"
#include "lvgl_pico.h"
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

void lvgl_pb_pico_configure_tick(void);
/* Embedded bytes remain immutable in XIP flash. The returned runtime owns its
 * widgets and workspace; borrowed trie bytes remain in the firmware image. */
bool lvgl_pb_pico_load_embedded(const char* name, const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output, char* error, size_t error_size);
/* Copies retained resources as needed, so input can be freed after return. */
bool lvgl_pb_pico_load_memory(const void* bytes, size_t size, const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output, char* error, size_t error_size);
/* Use an application-registered LVGL filesystem, for example S:/project.pb or
 * S:/project.xml. XML requires the optional codec enabled in this profile. */
bool lvgl_pb_pico_load_file(const char* path, const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output, char* error, size_t error_size);

#ifdef __cplusplus
}
#endif
#endif
