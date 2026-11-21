#pragma once
#include <lvgl.h>

namespace espos::keyboard {
// Presentation-only changes must not enter saved screens or undo history.
using KeyboardViewQuery = void (*)(lv_obj_t*, lv_point_t&, lv_point_t&, bool&);
using KeyboardStyleQuery = bool (*)(lv_obj_t*, lv_style_prop_t, bool&, lv_style_value_t&);
// Basic serializer-only builds do not link the optional keyboard implementation.
inline KeyboardViewQuery keyboard_view_query = nullptr;
inline KeyboardStyleQuery keyboard_style_query = nullptr;
inline void authored_keyboard_view(lv_obj_t* object, lv_point_t& position, lv_point_t& scroll, bool& floating)
{
    if (keyboard_view_query) keyboard_view_query(object, position, scroll, floating);
}
inline bool authored_keyboard_style(lv_obj_t* object, lv_style_prop_t property, bool& exists, lv_style_value_t& value)
{
    return keyboard_style_query && keyboard_style_query(object, property, exists, value);
}
}
