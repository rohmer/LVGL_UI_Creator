#include "lvgl_pb_arduino.h"
#include <Arduino.h>
#include <cstdlib>
#include <cstring>
namespace {
void Error(char* error,size_t capacity,const char* message)
{
    if (error && capacity) {std::strncpy(error,message,capacity-1);error[capacity-1]=0;}
}
uint32_t Milliseconds() {return static_cast<uint32_t>(millis());}
}
extern "C" void lvgl_pb_arduino_configure_tick(void) {lv_tick_set_cb(Milliseconds);}
extern "C" bool lvgl_pb_arduino_load_embedded(const char* name,const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output,char* error,size_t capacity)
{
    if (output) *output=nullptr;
    if (!name || !output) {Error(error,capacity,"Project name and output pointer are required");return false;}
    for (size_t index=0;index<lvgl_pb_embedded_project_count;++index)
        if (!std::strcmp(name,lvgl_pb_embedded_projects[index].name))
            return lvgl_runtime_load_memory_borrowed_assets(lvgl_pb_embedded_projects[index].data,
                lvgl_pb_embedded_projects[index].size,options,output,error,capacity);
    Error(error,capacity,"Embedded project name was not found");return false;
}
bool lvgl_pb_arduino_load_file(fs::FS& filesystem,const char* path,const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output,char* error,size_t capacity)
{
    if (output) *output=nullptr;
    if (!path || !output) {Error(error,capacity,"Filesystem path and output pointer are required");return false;}
    auto file=filesystem.open(path,FILE_READ);
    if (!file || file.isDirectory()) {Error(error,capacity,"Cannot open project file");return false;}
    const auto bytes=file.size();
    const size_t limit=options && options->max_input_bytes?options->max_input_bytes:16u*1024u*1024u;
    if (!bytes || bytes>limit) {Error(error,capacity,"project file is empty or exceeds the input limit");return false;}
    auto* buffer=static_cast<uint8_t*>(std::malloc(bytes));
    if (!buffer) {Error(error,capacity,"Cannot allocate project input buffer");return false;}
    const bool complete=file.read(buffer,bytes)==bytes;
    file.close();
    bool loaded=false;
    if (complete) loaded=lvgl_runtime_load_memory(buffer,bytes,options,output,error,capacity);
    else Error(error,capacity,"Could not read the complete project file");
    std::free(buffer);return loaded;
}
