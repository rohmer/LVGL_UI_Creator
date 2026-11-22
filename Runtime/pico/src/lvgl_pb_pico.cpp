#include "lvgl_pb_pico.h"
#include <cstring>

namespace {
void Error(char* output, size_t capacity, const char* message)
{
    if (output && capacity) { std::strncpy(output, message, capacity - 1); output[capacity - 1] = 0; }
}
}
extern "C" {
void lvgl_pb_pico_configure_tick(void) { lvgl_pico_configure_tick(); }
bool lvgl_pb_pico_load_embedded(const char* name, const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output, char* error, size_t capacity)
{
    if (output) *output = nullptr;
    if (!name || !output) { Error(error, capacity, "Project name and output pointer are required"); return false; }
    for (size_t index = 0; index < lvgl_pb_embedded_project_count; ++index)
        if (!std::strcmp(name, lvgl_pb_embedded_projects[index].name))
            return lvgl_runtime_load_memory_borrowed_assets(lvgl_pb_embedded_projects[index].data,
                lvgl_pb_embedded_projects[index].size, options, output, error, capacity);
    Error(error, capacity, "Embedded project name was not found"); return false;
}
bool lvgl_pb_pico_load_memory(const void* bytes, size_t size, const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output, char* error, size_t capacity)
{
    return lvgl_runtime_load_memory(bytes, size, options, output, error, capacity);
}
bool lvgl_pb_pico_load_file(const char* path, const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output, char* error, size_t capacity)
{
    return lvgl_runtime_load_file(path, options, output, error, capacity);
}
}
