#pragma once

#include <cstdint>

#include "lvgl.h"

namespace espos::keyboard {

struct KeyStyle {
    lv_color_t background = lv_color_white();
    lv_color_t foreground = lv_color_black();
    std::int32_t radius = 6;
    const lv_font_t* font = nullptr;  // A supplied font must outlive the key.
};

// Creates an LVGL button owned by parent, with a copied UTF-8 label.
// Returns nullptr for invalid input or a reported allocation failure.
// Call on the LVGL thread or while holding the same lock used by the UI.
[[nodiscard]] lv_obj_t* create_key(lv_obj_t* parent, const char* text, std::int32_t width,
                                   std::int32_t height, const KeyStyle& style = {});

}  // namespace espos::keyboard
