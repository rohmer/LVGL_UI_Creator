#pragma once
#include "../../Utils/EditorStyle.h"
#include <lvgl/lvgl.h>
#include <algorithm>

#if LVGL_VERSION_MAJOR >= 9
namespace TextInputModeControl
{
    inline constexpr const char* Options = "Alpha Numeric\nUpper case\nLower case\nNumeric";
    struct Hint { lv_obj_t* label; lv_obj_t* popup = nullptr; };

    inline void Hide(Hint* hint)
    {
        if (!hint->popup) return;
        auto* popup = hint->popup;
        hint->popup = nullptr;
        lv_obj_delete(popup);
    }

    inline void Event(lv_event_t* event)
    {
        auto* hint = static_cast<Hint*>(lv_event_get_user_data(event));
        const auto code = lv_event_get_code(event);
        if (code == LV_EVENT_DELETE) { Hide(hint); delete hint; return; }
        if (code == LV_EVENT_HOVER_LEAVE || code == LV_EVENT_PRESSED) { Hide(hint); return; }
        if (code != LV_EVENT_HOVER_OVER || hint->popup) return;
        auto* display = lv_obj_get_display(hint->label);
        auto* popup = hint->popup = lv_obj_create(lv_display_get_layer_sys(display));
        lv_obj_add_event_cb(popup, [](lv_event_t* e) {
            static_cast<Hint*>(lv_event_get_user_data(e))->popup = nullptr;
        }, LV_EVENT_DELETE, hint);
        lv_obj_set_size(popup, (std::min)(400, lv_display_get_horizontal_resolution(display) - 12), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(popup, 8, 0);
        lv_obj_set_style_bg_color(popup, lv_color_hex(EditorStyle::Input), 0);
        lv_obj_set_style_text_color(popup, lv_color_hex(EditorStyle::Text), 0);
        lv_obj_set_clickable(popup, false);
        lv_obj_set_scrollable(popup, false);
        auto* text = lv_label_create(popup);
        lv_obj_set_width(text, LV_PCT(100));
        lv_label_set_long_mode(text, LV_LABEL_LONG_MODE_WRAP);
        lv_label_set_text(text, "Alpha Numeric is unrestricted: letters, numbers, punctuation and Unicode. Upper case and Lower case convert letters automatically. Numeric accepts 0-9 and one decimal point. Character and length limits still apply. Advanced Keyboard follows the selected text box.");
        lv_obj_update_layout(popup);
        lv_area_t anchor{}; lv_obj_get_coords(hint->label, &anchor);
        const auto width = lv_obj_get_width(popup), height = lv_obj_get_height(popup);
        const int x = std::clamp(anchor.x1, 4, (std::max)(4, lv_display_get_horizontal_resolution(display) - width - 4));
        int y = anchor.y2 + 6;
        if (y + height > lv_display_get_vertical_resolution(display) - 4) y = anchor.y1 - height - 6;
        lv_obj_set_pos(popup, x, (std::max)(4, y));
    }

    inline void AddHint(lv_obj_t* label)
    {
        lv_obj_set_clickable(label, true);
        lv_obj_set_click_focusable(label, false);
        lv_obj_add_event_cb(label, Event, LV_EVENT_ALL, new Hint{label});
    }
}
#endif
