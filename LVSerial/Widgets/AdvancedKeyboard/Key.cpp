#include "espos_keyboard/Key.h"

namespace espos::keyboard {

lv_obj_t* create_key(lv_obj_t* parent, const char* text, std::int32_t width,
                     std::int32_t height, const KeyStyle& style)
{
    if (parent == nullptr || text == nullptr || width <= 0 || height <= 0 || style.radius < 0) {
        return nullptr;
    }

    lv_obj_t* button = lv_button_create(parent);
    if (button == nullptr) {
        return nullptr;
    }

    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, width, height);
    lv_obj_set_press_lock(button, false);
    lv_obj_set_scroll_on_focus(button, false);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_group_remove_obj(button); // Private touch keys must not steal keypad focus from the external field.

    constexpr lv_style_selector_t normal = LV_PART_MAIN;
    constexpr lv_style_selector_t pressed = normal | LV_STATE_PRESSED;
    lv_obj_set_style_radius(button, style.radius, normal);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, normal);
    lv_obj_set_style_bg_color(button, style.background, normal);
    lv_obj_set_style_bg_color(button, style.foreground, pressed);
    lv_obj_set_style_text_color(button, style.foreground, normal);
    lv_obj_set_style_text_color(button, style.background, pressed);
    if (style.font != nullptr) {
        lv_obj_set_style_text_font(button, style.font, normal);
    }

    lv_obj_t* label = lv_label_create(button);
    if (label == nullptr) {
        lv_obj_delete(button);
        return nullptr;
    }

    // The label inherits the button's current text color and leaves input to the button.
    lv_obj_remove_style_all(label);
    lv_obj_set_clickable(label, false);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, normal);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_text(label, text);
    if (lv_label_get_text(label) == nullptr) {
        lv_obj_delete(button);
        return nullptr;
    }
    lv_obj_center(label);

    return button;
}

}  // namespace espos::keyboard
