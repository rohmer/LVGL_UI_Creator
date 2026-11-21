#pragma once
#include <lvgl/lvgl.h>

// Shared editor chrome. Apply only to editor controls, never to project widgets.
namespace EditorStyle
{
constexpr unsigned Surface = 0x343b46, Header = 0x29313d, Input = 0x252d38;
constexpr unsigned Border = 0x526172, Text = 0xe4eaf2, Muted = 0xaebdce;
constexpr unsigned Accent = 0x58b8e8, Error = 0xf07878;
constexpr int ControlHeight = 36;

inline void Control(lv_obj_t* object)
{
    lv_obj_set_style_radius(object, 5, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(object, lv_color_hex(Input), LV_PART_MAIN);
    lv_obj_set_style_border_color(object, lv_color_hex(Border), LV_PART_MAIN);
    lv_obj_set_style_text_color(object, lv_color_hex(Text), LV_PART_MAIN);
    lv_obj_set_style_border_color(object, lv_color_hex(Accent), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(object, lv_color_hex(Accent), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(object, 2, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_opa(object, LV_OPA_50, LV_PART_MAIN | LV_STATE_DISABLED);
}

inline lv_obj_t* Help(lv_obj_t* parent, const char* text)
{
    auto* label = lv_label_create(parent);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(Muted), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(label, 3, LV_PART_MAIN);
    return label;
}

inline void Button(lv_obj_t* button)
{
    Control(button);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x395771), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x477b9c), LV_PART_MAIN | LV_STATE_PRESSED);
}
}
