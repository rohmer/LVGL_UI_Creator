#pragma once

#include <lvgl/lvgl.h>

#if LVGL_VERSION_MAJOR >= 9
#include <cstddef>
#include <Serialization/NativeStyles.h>
#include "EditorFocus.h"

using lv_obj_user_data_t = void*;

inline lv_obj_t* lv_obj_create(lv_obj_t* parent, std::nullptr_t) { return lv_obj_create(parent); }
inline lv_obj_t* lv_label_create(lv_obj_t* parent, std::nullptr_t) { return lv_label_create(parent); }
inline lv_obj_t* lv_roller_create(lv_obj_t* parent, std::nullptr_t) { return lv_roller_create(parent); }
inline lv_obj_t* lv_slider_create(lv_obj_t* parent, std::nullptr_t) { return lv_slider_create(parent); }
inline lv_obj_t* lv_list_create(lv_obj_t* parent, std::nullptr_t)
{
    auto* list = lv_obj_create(parent);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    return list;
}
inline lv_obj_t* lv_tabview_create(lv_obj_t* parent, std::nullptr_t) { return lv_tabview_create(parent); }
inline lv_obj_t* lv_btn_create(lv_obj_t* parent, std::nullptr_t) { return lv_button_create(parent); }
inline lv_obj_t* lv_ta_create(lv_obj_t* parent, std::nullptr_t) { return lv_textarea_create(parent); }
inline lv_obj_t* lv_kb_create(lv_obj_t* parent, std::nullptr_t) { return lv_keyboard_create(parent); }
inline lv_obj_t* lv_ddlist_create(lv_obj_t* parent, std::nullptr_t) { return lv_dropdown_create(parent); }
inline lv_obj_t* lv_cb_create(lv_obj_t* parent, std::nullptr_t) { return lv_checkbox_create(parent); }
inline lv_obj_t* lv_page_create(lv_obj_t* parent, std::nullptr_t) { return lv_obj_create(parent); }
inline lv_obj_t* lv_cont_create(lv_obj_t* parent, std::nullptr_t)
{
    auto* container = lv_obj_create(parent);
    lv_obj_set_scrollable(container, false);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(container, 8, LV_PART_MAIN);
    lv_obj_set_style_max_width(container, LV_PCT(100), LV_PART_MAIN);
    return container;
}

enum { LV_LAYOUT_OFF, LV_LAYOUT_COL_L = 10, LV_LAYOUT_PRETTY, LV_LAYOUT_ROW_M, LV_LAYOUT_ROW_T };
enum { LV_FIT_NONE, LV_FIT_TIGHT, LV_FIT_FILL };
enum { LV_CURSOR_NONE, LV_CURSOR_LINE };
enum { LV_SB_MODE_OFF = LV_SCROLLBAR_MODE_OFF };
enum { LV_TA_STYLE_BG, LV_PAGE_STYLE_BG, LV_CONT_STYLE_MAIN, LV_LABEL_STYLE_MAIN };
enum { LV_BTN_STYLE_REL, LV_BTN_STYLE_PR, LV_BTN_STYLE_TGL_REL, LV_BTN_STYLE_TGL_PR, LV_BTN_STYLE_INA };

inline void lv_cont_set_layout(lv_obj_t* object, int layout)
{
    if (layout == LV_LAYOUT_OFF)
    {
        lv_obj_set_layout(object, LV_LAYOUT_NONE);
        return;
    }
    const auto flow = layout == LV_LAYOUT_COL_L ? LV_FLEX_FLOW_COLUMN :
        LV_FLEX_FLOW_ROW_WRAP;
    lv_obj_set_flex_flow(object, flow);
    lv_obj_set_flex_align(object, layout == LV_LAYOUT_PRETTY ? LV_FLEX_ALIGN_SPACE_EVENLY : LV_FLEX_ALIGN_START,
                          layout == LV_LAYOUT_ROW_M ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
}

inline void lv_cont_set_fit2(lv_obj_t* object, int horizontal, int vertical)
{
    if (horizontal != LV_FIT_NONE)
        lv_obj_set_width(object, horizontal == LV_FIT_TIGHT ? LV_SIZE_CONTENT : LV_PCT(100));
    if (vertical != LV_FIT_NONE)
        lv_obj_set_height(object, LV_SIZE_CONTENT);
}

inline void lv_cont_set_fit(lv_obj_t* object, int fit) { lv_cont_set_fit2(object, fit, fit); }
inline void FitEditorPropertyRows(lv_obj_t* object)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
        FitEditorPropertyRows(lv_obj_get_child(object, i));
    if (lv_obj_get_class(object) == &lv_obj_class && lv_obj_get_style_layout(object, LV_PART_MAIN) == LV_LAYOUT_FLEX)
        lv_obj_set_height(object, LV_SIZE_CONTENT);
}
inline void lv_btn_set_layout(lv_obj_t* button, int layout) { lv_cont_set_layout(button, layout); }
inline void lv_obj_set_click(lv_obj_t* object, bool enabled) { lv_obj_set_clickable(object, enabled); }
inline void lv_obj_set_top(lv_obj_t* object, bool top) { if (top) lv_obj_move_foreground(object); }
inline lv_coord_t lv_obj_get_width_fit(lv_obj_t* object) { lv_obj_update_layout(object); return lv_obj_get_content_width(object); }
inline lv_coord_t lv_obj_get_height_fit(lv_obj_t* object) { lv_obj_update_layout(object); return lv_obj_get_content_height(object); }
inline void lv_obj_align(lv_obj_t* object, lv_obj_t* base, lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    if (base && base != lv_obj_get_parent(object))
        lv_obj_align_to(object, base, align, x, y);
    else
        lv_obj_align(object, align, x, y);
}

inline void lv_cb_set_checked(lv_obj_t* object, bool checked) { lv_obj_set_state(object, LV_STATE_CHECKED, checked); }
inline bool lv_cb_is_checked(lv_obj_t* object) { return lv_obj_has_state(object, LV_STATE_CHECKED); }
inline void HighlightEditorObject(lv_obj_t* object, bool selected)
{
    struct Highlight
    {
        lv_style_t style;
        Highlight()
        {
            lv_style_init(&style);
            lv_style_set_outline_color(&style, lv_color_hex(0x58b8e8));
            lv_style_set_outline_width(&style, 2);
            Serialization::NativeStyles::IgnoreTransientStyle(&style);
        }
    };
    static Highlight highlight;
    lv_obj_remove_style(object, &highlight.style, LV_PART_MAIN);
    if (selected)
        lv_obj_add_style(object, &highlight.style, LV_PART_MAIN);
}
inline void lv_ta_set_cursor_type(lv_obj_t* object, int type)
{
    lv_obj_set_style_opa(object, type == LV_CURSOR_NONE ? LV_OPA_TRANSP : LV_OPA_COVER,
                         LV_PART_CURSOR | LV_STATE_FOCUSED);
}
inline void lv_ta_set_sb_mode(lv_obj_t* object, int mode) { lv_obj_set_scrollbar_mode(object, static_cast<lv_scrollbar_mode_t>(mode)); }
inline void lv_page_set_sb_mode(lv_obj_t* object, int mode) { lv_ta_set_sb_mode(object, mode); }
inline void lv_ddlist_set_fix_width(lv_obj_t* object, lv_coord_t width) { lv_obj_set_width(object, width); }
inline void lv_ddlist_set_draw_arrow(lv_obj_t* object, bool enabled) { lv_dropdown_set_symbol(object, enabled ? LV_SYMBOL_DOWN : nullptr); }
inline void lv_label_set_align(lv_obj_t* object, lv_text_align_t align) { lv_obj_set_style_text_align(object, align, LV_PART_MAIN); }
inline void lv_roller_set_options(lv_obj_t* object, const char* options, bool infinite)
{
    lv_roller_set_options(object, options, infinite ? LV_ROLLER_MODE_INFINITE : LV_ROLLER_MODE_NORMAL);
}

namespace EditorStyles
{
    struct Styles
    {
        lv_style_t plain, accent, transparent, pressed, checked, checkedPressed, disabled, screen;

        Styles()
        {
            Initialize(plain, 0x383838, LV_OPA_COVER);
            Initialize(accent, 0x3b5c48, LV_OPA_COVER);
            Initialize(transparent, 0, LV_OPA_TRANSP);
            Initialize(pressed, 0x486653, LV_OPA_COVER);
            Initialize(checked, 0x45845b, LV_OPA_COVER);
            Initialize(checkedPressed, 0x346744, LV_OPA_COVER);
            Initialize(disabled, 0x454545, LV_OPA_COVER);
            Initialize(screen, 0x1e1e1e, LV_OPA_COVER);
        }

        static void Initialize(lv_style_t& style, uint32_t color, lv_opa_t opacity)
        {
            lv_style_init(&style);
            lv_style_set_bg_color(&style, lv_color_hex(color));
            lv_style_set_bg_opa(&style, opacity);
            lv_style_set_border_width(&style, 0);
            lv_style_set_text_color(&style, lv_color_hex(0xe0e0e0));
        }
    };

    inline Styles& Get()
    {
        static Styles styles;
        return styles;
    }
}

#define lv_style_plain        EditorStyles::Get().plain
#define lv_style_plain_color  EditorStyles::Get().accent
#define lv_style_pretty       EditorStyles::Get().plain
#define lv_style_pretty_color EditorStyles::Get().accent
#define lv_style_transp       EditorStyles::Get().transparent
#define lv_style_transp_fit   EditorStyles::Get().transparent
#define lv_style_transp_tight EditorStyles::Get().transparent
#define lv_style_scr          EditorStyles::Get().screen
#define lv_style_btn_rel      EditorStyles::Get().accent
#define lv_style_btn_pr       EditorStyles::Get().pressed
#define lv_style_btn_tgl_rel  EditorStyles::Get().checked
#define lv_style_btn_tgl_pr   EditorStyles::Get().checkedPressed
#define lv_style_btn_ina      EditorStyles::Get().disabled

inline void lv_obj_set_style(lv_obj_t* object, const lv_style_t* style)
{
    struct Binding
    {
        const lv_style_t* current;
        static void Delete(lv_event_t* event)
        {
            delete static_cast<Binding*>(lv_event_get_user_data(event));
        }
    };
    Binding* binding = nullptr;
    for (uint32_t i = 0; i < lv_obj_get_event_count(object); ++i)
    {
        auto* descriptor = lv_obj_get_event_dsc(object, i);
        if (lv_event_dsc_get_cb(descriptor) == Binding::Delete)
        {
            binding = static_cast<Binding*>(lv_event_dsc_get_user_data(descriptor));
            break;
        }
    }
    if (!binding)
    {
        binding = new Binding{};
        lv_obj_add_event_cb(object, Binding::Delete, LV_EVENT_DELETE, binding);
    }
    if (binding->current == style)
        return;
    if (binding->current)
        lv_obj_remove_style(object, binding->current, LV_PART_MAIN);
    binding->current = style;
    if (style)
        lv_obj_add_style(object, style, LV_PART_MAIN);
}

inline void lv_cont_set_style(lv_obj_t* object, int, const lv_style_t* style) { lv_obj_set_style(object, style); }
inline void lv_ta_set_style(lv_obj_t* object, int, const lv_style_t* style) { lv_obj_set_style(object, style); }
inline void lv_page_set_style(lv_obj_t* object, int, const lv_style_t* style) { lv_obj_set_style(object, style); }
inline void lv_label_set_style(lv_obj_t* object, int, const lv_style_t* style) { lv_obj_set_style(object, style); }
inline void lv_btn_set_style(lv_obj_t* object, int styleIndex, const lv_style_t* style)
{
    constexpr int states[] = {LV_STATE_DEFAULT, LV_STATE_PRESSED, LV_STATE_CHECKED,
        LV_STATE_CHECKED | LV_STATE_PRESSED, LV_STATE_DISABLED};
    lv_obj_add_style(object, style, LV_PART_MAIN | states[styleIndex]);
}

#define lv_cb_get_text lv_checkbox_get_text
#define lv_cb_set_text lv_checkbox_set_text
#define lv_ta_get_text lv_textarea_get_text
#define lv_ta_set_text lv_textarea_set_text
#define lv_ta_set_one_line lv_textarea_set_one_line
#define lv_ta_set_accepted_chars lv_textarea_set_accepted_chars
#define lv_kb_set_ta lv_keyboard_set_textarea
#define lv_ddlist_get_options lv_dropdown_get_options
#define lv_ddlist_get_selected lv_dropdown_get_selected
#define lv_ddlist_get_selected_str lv_dropdown_get_selected_str
#define lv_ddlist_set_options lv_dropdown_set_options
#define lv_ddlist_set_selected lv_dropdown_set_selected
#define lv_tabview_set_tab_act lv_tabview_set_active
#define lv_tabview_set_btns_pos lv_tabview_set_tab_bar_position
#define LV_TABVIEW_BTNS_POS_LEFT LV_DIR_LEFT
#define LV_ALIGN_IN_LEFT_MID LV_ALIGN_LEFT_MID
#define LV_LABEL_LONG_SROLL LV_LABEL_LONG_MODE_SCROLL
#define LV_LABEL_ALIGN_LEFT LV_TEXT_ALIGN_LEFT
#define lv_font_roboto_12 lv_font_montserrat_14
#endif

inline void SetEditorHidden(lv_obj_t* object, bool hidden)
{
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_hidden(object, hidden);
#else
    lv_obj_set_hidden(object, hidden);
#endif
}
