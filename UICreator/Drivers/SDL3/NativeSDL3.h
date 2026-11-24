#pragma once
#include <SDL3/SDL.h>
static inline bool uicreator_sdl3_primary_modifier(SDL_Keymod modifiers)
{
#if defined(__APPLE__)
    return (modifiers & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
#else
    return (modifiers & SDL_KMOD_CTRL) != 0;
#endif
}
#ifdef __cplusplus
extern "C" {
#endif
typedef bool (*uicreator_sdl3_shortcut_t)(SDL_Keycode key, SDL_Keymod modifiers, void* data);
void uicreator_sdl3_set_shortcut_handler(uicreator_sdl3_shortcut_t handler, void* data);
bool uicreator_sdl3_text_input_allowed(void);
typedef bool (*uicreator_sdl3_event_t)(const SDL_Event* event, void* data);
void uicreator_sdl3_set_event_handler(uicreator_sdl3_event_t handler, void* data);
bool uicreator_sdl3_quit_requested(void);
#ifdef __cplusplus
}
#endif
