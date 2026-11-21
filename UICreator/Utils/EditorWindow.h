#pragma once

#include <cstring>
#include <functional>
#include <lvgl/lvgl.h>
#include "EditorStyle.h"
#if LVGL_VERSION_MAJOR >= 9
#include "EditorIcons.h"
#endif

namespace EditorWindow
{
#if LVGL_VERSION_MAJOR >= 9
constexpr int HeaderHeight = 30;

inline lv_obj_t *Header(lv_obj_t *window)
{
    return lv_obj_get_child(window, 0);
}

inline lv_obj_t *Content(lv_obj_t *window)
{
    return lv_obj_get_child(window, 1);
}

struct CloseState
{
    lv_obj_t *window;
    lv_obj_t *button = nullptr;
    std::function<void()> close, show;
    bool external = false;
};
inline void DeleteCloseState(lv_event_t *event)
{
    delete static_cast<CloseState *>(lv_event_get_user_data(event));
}
inline CloseState *CloseData(lv_obj_t *window)
{
    for (uint32_t i = 0; i < lv_obj_get_event_count(window); ++i)
    {
        auto *event = lv_obj_get_event_dsc(window, i);
        if (lv_event_dsc_get_cb(event) == DeleteCloseState)
            return static_cast<CloseState *>(lv_event_dsc_get_user_data(event));
    }
    return nullptr;
}
inline const char *HeaderAction(const char *symbol)
{
    if (!symbol) return nullptr;
    const struct { const char *symbol; const char *action; } actions[] = {
        {LV_SYMBOL_CLOSE, "close"}, {LV_SYMBOL_DOWN, "move-down"},
        {LV_SYMBOL_UP, "move-up"}, {LV_SYMBOL_SETTINGS, "settings"},
        {LV_SYMBOL_COPY, "copy"}, {LV_SYMBOL_PASTE, "paste"},
        {LV_SYMBOL_TRASH, "delete"}, {LV_SYMBOL_PLUS, "add"},
        {LV_SYMBOL_LIST, "screen-flow"}
    };
    for (const auto &entry : actions)
        if (std::strcmp(symbol, entry.symbol) == 0) return entry.action;
    return nullptr;
}
inline lv_obj_t *HeaderButton(lv_obj_t *header, const char *symbol, const char *action = nullptr)
{
    auto *button = lv_button_create(header);
    lv_obj_set_size(button, 28, 28);
    EditorStyle::Button(button);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    auto *label = lv_label_create(button);
    lv_label_set_text(label, symbol);
    lv_obj_center(label);
    if (const auto *resolved = action ? action : HeaderAction(symbol))
        EditorIcons::Set(button, resolved, true, 18);
    return button;
}
inline lv_obj_t *CloseButton(lv_obj_t *window)
{
    auto *state = CloseData(window);
    return state ? state->button : nullptr;
}
inline void SetCloseHandler(lv_obj_t *window, std::function<void()> action)
{
    auto *state = CloseData(window);
    state->external = false;
    state->close = std::move(action);
}
inline void SetShowHandler(lv_obj_t *window, std::function<void()> action)
{
    CloseData(window)->show = std::move(action);
}
inline void Show(lv_obj_t *window)
{
    lv_obj_set_hidden(window, false);
    if (auto *state = CloseData(window))
        if (state->show)
        {
            auto action = state->show;
            action();
        }
}

inline void ReleaseFocus(lv_obj_t *window)
{
    for (auto *input = lv_indev_get_next(nullptr); input; input = lv_indev_get_next(input))
    {
        auto *group = lv_indev_get_group(input);
        if (!group)
            continue;
        while (auto *focused = lv_group_get_focused(group))
        {
            bool inside = false;
            for (auto *parent = focused; parent; parent = lv_obj_get_parent(parent))
                if (parent == window)
                {
                    inside = true;
                    break;
                }
            if (!inside)
                break;
            lv_group_remove_obj(focused);
        }
    }
}

inline lv_obj_t *Create(lv_obj_t *parent, bool closable = true)
{
    auto *window = lv_obj_create(parent);
    lv_obj_set_scrollable(window, false);
    lv_obj_set_flex_flow(window, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(window, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(window, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(window, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(window, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(window, lv_color_hex(EditorStyle::Border), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(window, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(window, lv_color_hex(EditorStyle::Surface), LV_PART_MAIN);
    lv_obj_set_style_text_color(window, lv_color_hex(EditorStyle::Text), LV_PART_MAIN);

    auto *header = lv_obj_create(window);
    lv_obj_set_size(header, LV_PCT(100), HeaderHeight);
    lv_obj_set_scrollable(header, false);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_left(header, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(header, lv_color_hex(EditorStyle::Header), LV_PART_MAIN);
    auto *title = lv_label_create(header);
    lv_obj_set_flex_grow(title, 1);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);

    auto *close = new CloseState{window};
    lv_obj_add_event_cb(window, DeleteCloseState, LV_EVENT_DELETE, close);
    if (closable)
    {
    close->button = HeaderButton(header, LV_SYMBOL_CLOSE);
    lv_obj_set_style_bg_opa(close->button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(close->button, lv_color_hex(0xa83b43), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(close->button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(
        close->button,
        [](lv_event_t *event) {
            auto *state = static_cast<CloseState *>(lv_event_get_user_data(event));
            ReleaseFocus(state->window);
            if (state->external)
                return;
            auto action = state->close;
            if (action)
                action();
            else
                lv_obj_set_hidden(state->window, true);
        },
        LV_EVENT_CLICKED, close);
    }

    auto *content = lv_obj_create(window);
    lv_obj_set_width(content, LV_PCT(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(content, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(content, lv_color_hex(EditorStyle::Surface), LV_PART_MAIN);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    return window;
}

inline void SetTitle(lv_obj_t *window, const char *text)
{
    lv_label_set_text(lv_obj_get_child(Header(window), 0), text);
}

inline lv_obj_t *AddButton(lv_obj_t *window, const char *symbol, const char *action = nullptr)
{
    auto *state = CloseData(window);
    if (state && symbol && std::strcmp(symbol, LV_SYMBOL_CLOSE) == 0)
    {
        state->external = true;
        if (action && state->button) EditorIcons::Set(state->button, action, true, 18);
        return state->button;
    }
    auto *button = HeaderButton(Header(window), symbol, action);
    if (state)
        lv_obj_move_to_index(button, (int32_t)lv_obj_get_child_count(Header(window)) - (state->button ? 2 : 1));
    return button;
}

inline int GetHeaderHeight(lv_obj_t *)
{
    return HeaderHeight;
}

inline void HideScrollbar(lv_obj_t *window)
{
    lv_obj_set_scrollbar_mode(Content(window), LV_SCROLLBAR_MODE_OFF);
}
#else
inline lv_obj_t *Create(lv_obj_t *parent, bool = true)
{
    return lv_win_create(parent, nullptr);
}
inline lv_obj_t *Content(lv_obj_t *window)
{
    return window;
}
inline lv_obj_t *Header(lv_obj_t *window)
{
    return window;
}
inline void SetTitle(lv_obj_t *window, const char *text)
{
    lv_win_set_title(window, text);
}
inline lv_obj_t *AddButton(lv_obj_t *window, const char *symbol, const char * = nullptr)
{
    return lv_win_add_btn(window, symbol);
}
inline int GetHeaderHeight(lv_obj_t *window)
{
    return lv_win_get_btn_size(window);
}
inline void HideScrollbar(lv_obj_t *window)
{
    lv_win_set_sb_mode(window, LV_SB_MODE_OFF);
}
#endif
} // namespace EditorWindow
