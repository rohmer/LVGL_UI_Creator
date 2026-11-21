#pragma once
#include <lvgl/lvgl.h>

// Shared editor button decoration. Existing labels and callbacks stay on the
// button; vector children never receive pointer or keyboard focus.
namespace EditorIcons {
bool Set(lv_obj_t* button, const char* action, bool iconOnly = false, int size = 20);
lv_obj_t* Icon(lv_obj_t* button);
const char* Action(lv_obj_t* button);
const char* Hint(lv_obj_t* button);
}
