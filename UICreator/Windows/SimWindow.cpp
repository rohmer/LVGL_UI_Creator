#include "SimWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorDrag.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorEvents.h"
#include <algorithm>
#include <charconv>
#include <cstring>
#if LVGL_VERSION_MAJOR >= 9
#include <src/core/lv_obj_class_private.h>
#endif

namespace {
#if LVGL_VERSION_MAJOR >= 9
size_t WidgetInstanceBytes(const lv_obj_t* object)
{
    // LVGL allocates the first nonzero instance_size in the class chain.
    // That size includes the base object; inherited classes are not additive.
    const auto* widgetClass = lv_obj_get_class(object);
    while (widgetClass && !widgetClass->instance_size) widgetClass = widgetClass->base_class;
    size_t bytes = widgetClass ? widgetClass->instance_size : 0;
    const auto count = lv_obj_get_child_count(object);
    for (uint32_t index = 0; index < count; ++index)
        bytes += WidgetInstanceBytes(lv_obj_get_child(object, index));
    return bytes;
}
#endif
bool ReadDimension(lv_obj_t* field, int& value)
{
    const char* text = lv_ta_get_text(field);
    const char* end = text + std::strlen(text);
    const auto result = std::from_chars(text, end, value);
    return result.ec == std::errc{} && result.ptr == end && value > 0;
}
}

SimWindow::SimWindow(int iWidth, int iHeight) :
    width(iWidth),
    height(iHeight)
{
    createObjects();
}

int SimWindow::updateFrame()
{
    std::stringstream title;
    title << (width < 200 ? "Simulator\n[Size:" : "Simulator [Size:") << width << "x" << height << "]";
    EditorWindow::SetTitle(window, title.str().c_str());
#if LVGL_VERSION_MAJOR >= 9
    // Keep the byte count on its own line so it remains readable at TFT sizes.
    auto* header = EditorWindow::Header(window);
    const int lineHeight = lv_font_get_line_height(lv_obj_get_style_text_font(header, LV_PART_MAIN));
    const int titleHeight = std::max(28, lineHeight * (width < 200 ? 2 : 1));
    const int headerSize = 12 + titleHeight + 4 + lineHeight;
    lv_obj_set_height(header, headerSize);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return headerSize;
#else
    return EditorWindow::GetHeaderHeight(window);
#endif
}

void SimWindow::RefreshMemoryUsage()
{
#if LVGL_VERSION_MAJOR >= 9
    if (!memoryLabel) return;
    const auto bytes = WidgetInstanceBytes(drawSurface);
    if (bytes == memoryBytes) return;
    memoryBytes = bytes;
    lv_label_set_text_fmt(memoryLabel, "Screen Memory: %zu bytes", bytes);
#endif
}

void SimWindow::resize(int newWidth, int newHeight, bool repos, bool resize)
{
    EditorEdit::Scope edit;
    float xDelta = static_cast<float>(newWidth) / static_cast<float>(width);
    float yDelta = static_cast<float>(newHeight) / static_cast<float>(height);

    width = newWidth;
    height = newHeight;
    const int headerSize = updateFrame();
    lv_obj_set_size(window, width + 12, height + headerSize + 12);
    lv_obj_set_size(drawSurface, width, height);
    if (repos || resize)
    {
        std::vector<lv_obj_t*> children = ObjectTools::GetChildren(drawSurface);
        for (std::vector<lv_obj_t*>::iterator it = children.begin(); it != children.end(); ++it)
        {
            if (repos)
            {
                lv_obj_set_pos(*it,
                               round((lv_obj_get_x(*it) * xDelta)),
                               round((lv_obj_get_y(*it) * yDelta))
                );
            }
            if (resize)
            {
                lv_obj_set_size(*it,
                                round((lv_obj_get_width(*it) * xDelta)),
                                round((lv_obj_get_height(*it) * yDelta))
                );
            }
        }
    }
}

void SimWindow::createObjects()
{
    window = EditorWindow::Create(lv_scr_act(), false);
#if LVGL_VERSION_MAJOR >= 9
    EditorDrag::Attach(EditorWindow::Header(window), window);
#endif
    const int headerSize = updateFrame();

    lv_obj_set_size(window, width + 12, height + headerSize + 12);
    lv_area_t screenBounds;
    lv_obj_get_coords(lv_scr_act(), &screenBounds);
    int x = (screenBounds.x2 / 2) - (width / 2);
    int y = (screenBounds.y2 / 2) - (width / 2) - 50;
#if LVGL_VERSION_MAJOR >= 9
    const int workspaceLeft = 528;
    const int workspaceRight = screenBounds.x2 + 1 - 405;
    x = workspaceLeft + (workspaceRight - workspaceLeft - width - 12) / 2;
    y = 136;
#endif
    lv_obj_set_pos(window, x, y);
    EditorWindow::HideScrollbar(window);
    lv_obj_t* winBtn = EditorWindow::AddButton(window, LV_SYMBOL_SETTINGS, "settings");
    lv_obj_set_user_data(winBtn, this);
    SetEditorEventCallback<settings>(winBtn);
#if LVGL_VERSION_MAJOR >= 9
    memoryLabel = lv_label_create(EditorWindow::Header(window));
    lv_obj_set_ignore_layout(memoryLabel, true);
    lv_obj_set_width(memoryLabel, LV_PCT(100));
    lv_label_set_long_mode(memoryLabel, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(memoryLabel, lv_color_hex(EditorStyle::Muted), LV_PART_MAIN);
    lv_obj_align(memoryLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
#endif
    drawSurface = lv_cont_create(EditorWindow::Content(window), nullptr);
    lv_obj_set_style(drawSurface, &lv_style_transp);
    lv_cont_set_layout(drawSurface, LV_LAYOUT_OFF);
#if LVGL_VERSION_MAJOR < 9
    lv_obj_set_auto_realign(drawSurface, false);
#endif
    lv_obj_set_size(drawSurface, width, height);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_pos(drawSurface, 5, 5);
#else
    lv_obj_set_x(drawSurface, lv_obj_get_x(drawSurface) - 3);
    lv_obj_set_y(drawSurface, lv_obj_get_y(drawSurface) - 6);
#endif
    RefreshMemoryUsage();
}

void SimWindow::settings(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    SimWindow* sw = static_cast<SimWindow*>(lv_obj_get_user_data(obj));

    if (sw->settingsWindow != nullptr)
    {
        SetEditorHidden(sw->settingsWindow, false);
        lv_obj_move_foreground(sw->settingsWindow);
        auto* controls = static_cast<cbStruct*>(lv_obj_get_user_data(sw->settingsWindow));
        focusDimension(controls, controls->width);
        return;
    }
    sw->settingsWindow = EditorWindow::Create(lv_scr_act());
    lv_obj_set_size(sw->settingsWindow, 400, 450);
    EditorWindow::SetTitle(sw->settingsWindow, "TFT Settings");
    lv_area_t screenBounds;
    lv_obj_get_coords(lv_scr_act(), &screenBounds);
    int x = (screenBounds.x2 / 2) - 150;
    int y = (screenBounds.y2 / 2) - 150;
    lv_obj_set_pos(sw->settingsWindow, x, y);
    lv_obj_t* close_btn = EditorWindow::AddButton(sw->settingsWindow, LV_SYMBOL_CLOSE);
    lv_obj_set_user_data(close_btn, sw->settingsWindow);
    SetEditorEventCallback<closeSettings>(close_btn);

    auto* content = EditorWindow::Content(sw->settingsWindow);
    lv_obj_t* wLabel = lv_label_create(content, nullptr);
    lv_obj_set_pos(wLabel, 56, 23);
    lv_label_set_text(wLabel, "TFT Width:");
    lv_obj_t* hLabel = lv_label_create(content, nullptr);
    lv_obj_set_pos(hLabel, 50, 73);
    lv_label_set_text(hLabel, "TFT Height:");

    lv_obj_t* wTA = lv_ta_create(content, nullptr);
    lv_ta_set_accepted_chars(wTA, "0123456789");
    lv_obj_set_pos(wTA, 150, 12);
    std::stringstream widthSS;
    lv_obj_set_width(wTA, 75);
    widthSS << sw->width;
    lv_ta_set_text(wTA, widthSS.str().c_str());
    lv_ta_set_one_line(wTA, true);

    lv_obj_t* hTA = lv_ta_create(content, nullptr);
    lv_ta_set_accepted_chars(hTA, "0123456789");
    lv_obj_set_pos(hTA, 150, 63);
    std::stringstream heightSS;
    heightSS << sw->height;
    lv_obj_set_width(hTA, 75);
    lv_ta_set_text(hTA, heightSS.str().c_str());
    lv_ta_set_one_line(hTA, true);

    lv_ta_set_cursor_type(wTA, LV_CURSOR_NONE);
    lv_ta_set_cursor_type(hTA, LV_CURSOR_NONE);

    lv_obj_t* kb = lv_kb_create(content, nullptr);
    lv_obj_set_y(kb, lv_obj_get_y(kb) - 10);
#if LVGL_VERSION_MAJOR >= 9
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
#else
    static const char* kb_map_num[] =
    {
        "7", "8", "9", "\n",
        "4", "5", "6", "\n",
        "1", "2", "3", "\n",
        LV_SYMBOL_LEFT, "0",LV_SYMBOL_RIGHT, "\n",
        "Bksp", ""
    };
    static const lv_btnm_ctrl_t kb_ctrl_num[] =
    {
        1, 1, 1,
        1, 1, 1,
        1, 1, 1,
        LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG | 1, 1, LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG | 1,
        LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG | 2
    };
    lv_kb_set_map(kb, kb_map_num);
    lv_kb_set_ctrl_map(kb, kb_ctrl_num);
#endif

    SetEditorHidden(kb, true);

    lv_obj_t* pos = lv_cb_create(content, nullptr);
    lv_cb_set_text(pos, "Reposition objects");
    lv_cb_set_checked(pos, true);
    lv_obj_set_pos(pos, 20, 110);

    lv_obj_t* sze = lv_cb_create(content, nullptr);
    lv_cb_set_text(sze, "Resize objects");
    lv_cb_set_checked(sze, true);
    lv_obj_set_pos(sze, 240, 110);

    lv_obj_t* submitBtn = lv_btn_create(content, nullptr);
    lv_obj_t* btnLabel = lv_label_create(submitBtn, nullptr);
    lv_label_set_text(btnLabel, "Submit Changes");
    lv_obj_set_pos(submitBtn, 25, 150);
    lv_obj_set_size(submitBtn, 350, 30);
    SetEditorHidden(submitBtn, true);

    cbStruct* cb = new cbStruct();
    cb->sw = sw;
    cb->height = hTA;
    cb->width = wTA;
    cb->sze = sze;
    cb->pos = pos;
    cb->setBtn = submitBtn;
    cb->kb = kb;
    lv_obj_set_user_data(sw->settingsWindow, cb);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_add_event_cb(sw->settingsWindow, [](lv_event_t* event) {
        auto* controls = static_cast<cbStruct*>(lv_event_get_user_data(event));
        // LVGL destroys input devices before windows during shutdown. Remove
        // both fields directly while their callbacks still have live controls.
        lv_group_remove_obj(controls->width);
        lv_group_remove_obj(controls->height);
        lv_kb_set_ta(controls->kb, nullptr);
        controls->sw->settingsWindow = nullptr;
        delete controls;
    }, LV_EVENT_DELETE, cb);
#endif

    lv_obj_set_user_data(wTA, cb);
    lv_obj_set_user_data(hTA, cb);
    lv_obj_set_user_data(submitBtn, cb);
    SetEditorEventCallback<submitClicked>(submitBtn);

    SetEditorEventCallback<dimensionChanged>(wTA);
    SetEditorEventCallback<dimensionChanged>(hTA);
    focusDimension(cb, wTA);
}

void SimWindow::closeSettings(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    lv_obj_t* win = static_cast<lv_obj_t*>(lv_obj_get_user_data(obj));
#if LVGL_VERSION_MAJOR >= 9
    EditorWindow::ReleaseFocus(win);
#endif
    auto* controls = static_cast<cbStruct*>(lv_obj_get_user_data(win));
    lv_kb_set_ta(controls->kb, nullptr);
    SetEditorHidden(win, true);
}

void SimWindow::focusDimension(cbStruct* controls, lv_obj_t* field)
{
#if LVGL_VERSION_MAJOR >= 9
    if (auto* group = controls->sw->keyboardGroup) {
        FocusEditorControl(group, field);
        // Keep both dimensions in the group so Tab can switch between them.
        // FOCUSED events only retarget the keypad; they must not add fields
        // back while ReleaseFocus is removing a closing dialog's controls.
        auto* peer = field == controls->width ? controls->height : controls->width;
        if (lv_obj_get_group(peer) != group) lv_group_add_obj(group, peer);
        lv_group_set_editing(group, true);
    }
#endif
    SetEditorHidden(controls->kb, false);
    lv_ta_set_cursor_type(controls->width, field == controls->width ? LV_CURSOR_LINE : LV_CURSOR_NONE);
    lv_ta_set_cursor_type(controls->height, field == controls->height ? LV_CURSOR_LINE : LV_CURSOR_NONE);
    lv_kb_set_ta(controls->kb, field);
}

void SimWindow::dimensionChanged(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED && event != LV_EVENT_FOCUSED &&
        event != LV_EVENT_VALUE_CHANGED && event != LV_EVENT_READY)
        return;
    cbStruct* cb = static_cast<cbStruct*>(lv_obj_get_user_data(obj));
    int width = 0, height = 0;
    const bool changed = ReadDimension(cb->width, width) && ReadDimension(cb->height, height) &&
        (width != cb->sw->width || height != cb->sw->height);
    SetEditorHidden(cb->setBtn, !changed);
    if (event == LV_EVENT_CLICKED) focusDimension(cb, obj);
    else if (event == LV_EVENT_FOCUSED) {
        SetEditorHidden(cb->kb, false);
        lv_ta_set_cursor_type(cb->width, obj == cb->width ? LV_CURSOR_LINE : LV_CURSOR_NONE);
        lv_ta_set_cursor_type(cb->height, obj == cb->height ? LV_CURSOR_LINE : LV_CURSOR_NONE);
        lv_kb_set_ta(cb->kb, obj);
    }
    else if (event == LV_EVENT_READY && changed) submitClicked(cb->setBtn, LV_EVENT_CLICKED);
}

void SimWindow::submitClicked(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    cbStruct* cb = static_cast<cbStruct*>(lv_obj_get_user_data(obj));
    int width = 0, height = 0;
    if (!ReadDimension(cb->width, width) || !ReadDimension(cb->height, height))
        return;
    cb->sw->resize(width, height, lv_cb_is_checked(cb->pos), lv_cb_is_checked(cb->sze));
#if LVGL_VERSION_MAJOR >= 9
    EditorWindow::ReleaseFocus(cb->sw->settingsWindow);
#endif
    lv_kb_set_ta(cb->kb, nullptr);
    SetEditorHidden(cb->sw->settingsWindow, true);
}
