#include "lvgl_pb_esp32.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#else
#include <chrono>
#endif
namespace {
void Error(char* error, size_t capacity, const char* text) {
    if(error && capacity) { std::strncpy(error,text,capacity-1);error[capacity-1]=0; }
}
uint32_t Milliseconds() {
#ifdef ESP_PLATFORM
    return static_cast<uint32_t>(esp_timer_get_time()/1000);
#else
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
}
extern "C" void lvgl_pb_esp32_configure_tick(void) { lv_tick_set_cb(Milliseconds); }
extern "C" bool lvgl_pb_esp32_load_embedded(const char* name,const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output,char* error,size_t capacity) {
    if(output)*output=nullptr;
    if(!name || !output) {Error(error,capacity,"Project name and output pointer are required");return false;}
    for(size_t i=0;i<lvgl_pb_embedded_project_count;++i)
        if(!std::strcmp(name,lvgl_pb_embedded_projects[i].name))
            return lvgl_runtime_load_memory_borrowed_assets(lvgl_pb_embedded_projects[i].data,
                lvgl_pb_embedded_projects[i].size,options,output,error,capacity);
    Error(error,capacity,"Embedded project name was not found");return false;
}
extern "C" bool lvgl_pb_esp32_load_file(const char* path,const lvgl_runtime_options_t* options,
    lvgl_runtime_t** output,char* error,size_t capacity) {
    if(output)*output=nullptr;
    if(!path || !output) {Error(error,capacity,"VFS path and output pointer are required");return false;}
    auto* file=std::fopen(path,"rb");
    if(!file){Error(error,capacity,"Cannot open project VFS file");return false;}
    const size_t limit=options && options->max_input_bytes ? options->max_input_bytes : 16u*1024u*1024u;
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);Error(error,capacity,"Cannot seek project VFS file");return false;}
    const long length=std::ftell(file);
    if(length<=0 || static_cast<unsigned long long>(length)>limit || std::fseek(file,0,SEEK_SET)!=0) {
        std::fclose(file);Error(error,capacity,"project VFS file is empty, too large or not seekable");return false;
    }
    auto* bytes=static_cast<unsigned char*>(std::malloc(static_cast<size_t>(length)));
    if(!bytes){std::fclose(file);Error(error,capacity,"Cannot allocate project input buffer");return false;}
    const bool complete=std::fread(bytes,1,static_cast<size_t>(length),file)==static_cast<size_t>(length);
    std::fclose(file);
    bool loaded=false;
    if(complete)loaded=lvgl_runtime_load_memory(bytes,static_cast<size_t>(length),options,output,error,capacity);
    else Error(error,capacity,"Could not read complete project VFS file");
    std::free(bytes);return loaded;
}
