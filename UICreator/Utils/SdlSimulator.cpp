#include "SdlSimulator.h"

#if LVGL_VERSION_MAJOR >= 9
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "../Drivers/SDL3/NativeSDL3.h"
#include <algorithm>
#include <string_view>

bool SdlSimulator::Initialize(int width, int height)
{
#if defined(__linux__)
    SDL_SetAppMetadata("LVGL UI Creator", nullptr, "lvgl-ui-creator");
#endif
    SDL_SetMainReady();
    lv_init();
    initialized = true;
    display = lv_sdl_window_create(width, height);
    if (!display)
        return false;

    lv_display_add_event_cb(display, [](lv_event_t* event) {
        auto* simulator = static_cast<SdlSimulator*>(lv_event_get_user_data(event));
        simulator->display = nullptr;
    }, LV_EVENT_DELETE, this);
    lv_sdl_window_set_title(display, "LVGL UI Creator");
    if (SDL_GetCurrentVideoDriver() && std::string_view(SDL_GetCurrentVideoDriver()) != "dummy")
    {
        auto* window = lv_sdl_window_get_window(display);
        if (!SDL_SetWindowFullscreen(window, true))
            return false;
        int fullscreenWidth = width, fullscreenHeight = height;
        SDL_GetWindowSize(window, &fullscreenWidth, &fullscreenHeight);
        lv_display_set_resolution(display, fullscreenWidth, fullscreenHeight);
    }

    if (!lv_sdl_mouse_create())
        return false;
    keyboard = lv_sdl_keyboard_create();
    return keyboard != nullptr;
}

bool SdlSimulator::ProcessEvents()
{
    if (!display)
        return false;
    const uint32_t delay = lv_timer_handler();
    if (!display || uicreator_sdl3_quit_requested())
        return false;
    SDL_Delay(std::clamp(delay, 1u, 5u));
    return true;
}

void SdlSimulator::SetShortcutHandler(std::function<bool(int,bool,bool,bool)> handler) {
    shortcutHandler=std::move(handler);
    uicreator_sdl3_set_shortcut_handler([](SDL_Keycode key,SDL_Keymod modifiers,void* context)->bool {
        auto* self=static_cast<SdlSimulator*>(context);
        int code=(int)key;if(code>='A' && code<='Z')code=code-'A'+'a';
        return self->shortcutHandler && self->shortcutHandler(code,(modifiers&SDL_KMOD_CTRL)!=0,(modifiers&SDL_KMOD_SHIFT)!=0,(modifiers&SDL_KMOD_ALT)!=0);
    },this);
}
SdlSimulator::~SdlSimulator()
{
    if (initialized)
    {
        uicreator_sdl3_set_shortcut_handler(nullptr,nullptr);
        if (lv_is_initialized()) {
            lv_sdl_quit(); // Stop event polling before deleting document objects.
            lv_deinit();
        }
        SDL_Quit();
    }
}
#endif
