#pragma once
#include <SDL3/SDL.h>
#include <lvgl/lvgl.h>

// Desktop numeric entry adapter. Arrow keys keep LVGL's native digit/step behavior.
namespace EditorSpinbox {
bool HandleEvent(const SDL_Event& event, lv_group_t* group);
}
