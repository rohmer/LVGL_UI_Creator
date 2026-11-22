#define SDL_MAIN_HANDLED
#include "lvgl_pb_display.h"
#include "lvgl_runtime.h"
#include "../../UICreator/Drivers/SDL3/NativeSDL3.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <lvgl/lvgl.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

struct lvgl_pb_window {
    lv_display_t* display = nullptr;
    lvgl_runtime_t* runtime = nullptr;
    lv_group_t* keyboardGroup = nullptr;
    std::string error;
};
namespace {
std::atomic<bool> owned{false};
constexpr size_t InputLimit = 16 * 1024 * 1024;
void Error(char* output, size_t capacity, const char* text) {
    if (!output || !capacity) return;
    const auto count = (std::min)(capacity - 1, std::strlen(text));
    std::memcpy(output, text, count); output[count] = 0;
}
}
lvgl_pb_window_t* lvgl_pb_window_create(int width, int height, const char* title, char* error, size_t size) {
    if (width < 1 || height < 1 || width > 8192 || height > 8192) { Error(error,size,"Invalid display dimensions (1-8192)"); return nullptr; }
    if (owned.exchange(true)) { Error(error,size,"A Protobuf display window already exists in this process"); return nullptr; }
    std::unique_ptr<lvgl_pb_window> window;
    try {
        window = std::make_unique<lvgl_pb_window>();
        SDL_SetMainReady(); lv_init();
        window->display = lv_sdl_window_create(width, height);
        if (!window->display) throw std::runtime_error(SDL_GetError());
        lv_display_add_event_cb(window->display, [](lv_event_t* event) {
            static_cast<lvgl_pb_window*>(lv_event_get_user_data(event))->display = nullptr;
        }, LV_EVENT_DELETE, window.get());
        lv_sdl_window_set_title(window->display,title ? title : "LVGL Protobuf Display");
        auto* mouse=lv_sdl_mouse_create();auto* keyboard=lv_sdl_keyboard_create();
        if (!mouse || !keyboard) throw std::runtime_error("Cannot create native input devices");
        window->keyboardGroup=lv_group_create();
        if (!window->keyboardGroup) throw std::runtime_error("Cannot create keyboard focus group");
        lv_group_set_default(window->keyboardGroup);lv_indev_set_group(keyboard,window->keyboardGroup);
        Error(error,size,""); return window.release();
    } catch (const std::exception& failure) {
        Error(error,size,failure.what());
        if (lv_is_initialized()) { lv_sdl_quit(); lv_deinit(); }
        SDL_Quit(); owned = false; return nullptr;
    }
}
void lvgl_pb_window_destroy(lvgl_pb_window_t* window) {
    if (!window) return;
    lvgl_runtime_destroy(window->runtime); window->runtime = nullptr;
    if (lv_is_initialized()) { lv_sdl_quit(); lv_deinit(); }
    SDL_Quit(); delete window; owned = false;
}
bool lvgl_pb_window_load_memory(lvgl_pb_window_t* window, const void* bytes, size_t size) {
    if (!window || !window->display) return false;
    char error[512]{}; lvgl_runtime_t* candidate = nullptr;
    const lvgl_runtime_options_t options{window->display,InputLimit,true};
    if (!lvgl_runtime_load_memory(bytes,size,&options,&candidate,error,sizeof error)) { window->error=error; return false; }
    lvgl_runtime_destroy(window->runtime); window->runtime=candidate; window->error.clear();
    // Use document dimensions without rescaling saved object coordinates.
    int32_t width=0,height=0;
    // Resolve the selected screen from the runtime's authored dimensions,
    // before display resizing can change native screen bounds.
    for(size_t index=0;index<lvgl_runtime_screen_count(candidate);++index) {
        if(lvgl_runtime_screen(candidate,index)!=lv_display_get_screen_active(window->display))continue;
        lvgl_runtime_screen_size(candidate,index,&width,&height);
        if(width>0 && height>0 && width<=8192 && height<=8192) {
            SDL_SetWindowSize(lv_sdl_window_get_window(window->display),width,height);
            lv_display_set_resolution(window->display,width,height);
        }
        break;
    }
    return true;
}
bool lvgl_pb_window_load_file(lvgl_pb_window_t* window, const char* path) {
    if (!window || !path) return false;
    try {
        std::ifstream input(std::filesystem::u8path(path),std::ios::binary|std::ios::ate);
        if (!input) throw std::runtime_error("Cannot open project file");
        const auto count=input.tellg();
        if(count<=0 || static_cast<uint64_t>(count)>InputLimit) throw std::runtime_error("project file must contain 1 byte to 16 MiB");
        std::vector<char> bytes(static_cast<size_t>(count)); input.seekg(0);input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        if(!input) throw std::runtime_error("Cannot read complete project file");
        return lvgl_pb_window_load_memory(window,bytes.data(),bytes.size());
    } catch(const std::exception& error) { window->error=error.what();return false; }
}
bool lvgl_pb_window_poll(lvgl_pb_window_t* window) {
    if (!window || !window->display || uicreator_sdl3_quit_requested()) return false;
    const auto delay=lv_timer_handler();
    if(!window->display || uicreator_sdl3_quit_requested()) return false;
    SDL_Delay(std::clamp(delay,1u,5u)); return true;
}
const char* lvgl_pb_window_error(const lvgl_pb_window_t* window) { return window ? window->error.c_str() : "No display window"; }
size_t lvgl_pb_window_screen_count(const lvgl_pb_window_t* window) { return window ? lvgl_runtime_screen_count(window->runtime) : 0; }
bool lvgl_pb_window_active_screen(const lvgl_pb_window_t* window,size_t* screen) {
    if(!window || !window->display || !window->runtime || !screen) return false;
    const auto* active=lv_display_get_screen_active(window->display);
    if(!active) return false;
    for(size_t index=0;index<lvgl_runtime_screen_count(window->runtime);++index) {
        if(lvgl_runtime_screen(window->runtime,index)!=active) continue;
        *screen=index;return true;
    }
    return false;
}
bool lvgl_pb_window_show_screen(lvgl_pb_window_t* window,size_t screen) {
    if(!window || !lvgl_runtime_show_screen(window->runtime,screen))return false;
    int32_t width=0,height=0;
    if(lvgl_runtime_screen_size(window->runtime,screen,&width,&height) && width>0 && height>0 && width<=8192 && height<=8192) {
        SDL_SetWindowSize(lv_sdl_window_get_window(window->display),width,height);
        lv_display_set_resolution(window->display,width,height);
    }
    return true;
}
void* lvgl_pb_window_object(lvgl_pb_window_t* window,size_t screen,int32_t id) { return window ? lvgl_runtime_object(window->runtime,screen,id) : nullptr; }
size_t lvgl_pb_window_variable_count(const lvgl_pb_window_t* window) { return window ? lvgl_runtime_variable_count(window->runtime) : 0; }
const char* lvgl_pb_window_variable_name(const lvgl_pb_window_t* window,size_t index) { return window ? lvgl_runtime_variable_name(window->runtime,index) : nullptr; }
bool lvgl_pb_window_get_variable(lvgl_pb_window_t* window,const char* name,lvgl_pb_value_t* value) {
    if(!window || !value) return false;
    lvgl_runtime_value_t native{};
    if(!lvgl_runtime_get_variable(window->runtime,name,&native)) return false;
    *value={static_cast<lvgl_pb_value_type_t>(native.type),native.string_value,native.integer_value,native.real_value,native.boolean_value};return true;
}
bool lvgl_pb_window_set_variable(lvgl_pb_window_t* window,const char* name,const lvgl_pb_value_t* value) {
    if(!window || !value || value->type<LVGL_PB_STRING || value->type>LVGL_PB_DATETIME) return false;
    const lvgl_runtime_value_t native{static_cast<lvgl_runtime_value_type_t>(value->type),value->string_value,value->integer_value,value->real_value,value->boolean_value};
    return lvgl_runtime_set_variable(window->runtime,name,&native);
}
bool lvgl_pb_window_save_bmp(lvgl_pb_window_t* window,const char* path) {
    if(!window || !window->display || !path) return false;
    lv_refr_now(window->display);
    auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(window->display));
    auto* surface=SDL_RenderReadPixels(renderer,nullptr);
    if(!surface) {window->error=SDL_GetError();return false;}
    const bool result=SDL_SaveBMP(surface,path);SDL_DestroySurface(surface);
    if(!result)window->error=SDL_GetError();return result;
}
void lvgl_pb_window_set_title(lvgl_pb_window_t* window,const char* title) {
    if(window && window->display && title)lv_sdl_window_set_title(window->display,title);
}
