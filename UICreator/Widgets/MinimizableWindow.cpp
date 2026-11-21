#include "MinimizableWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorEvents.h"

MinimizableWindow::MinimizableWindow(std::string title,
                                     int x,
                                     int y,
                                     int width,
                                     int height,
                                     lv_obj_t* parent,
                                     bool hasCloseBtn,
                                     int iconX,
                                     int iconY,
                                     int iconWidth,
                                     int iconHeight)
{
    if (parent == nullptr)
        parent = lv_scr_act();
    windowObject = EditorWindow::Create(parent);
#if LVGL_VERSION_MAJOR >= 9
    y += 36;
    height = std::min(height, int(lv_display_get_vertical_resolution(nullptr)) - y);
#endif
    lv_obj_set_size(windowObject, width, height);
    lv_obj_set_pos(windowObject, x, y);
    EditorWindow::SetTitle(windowObject, title.c_str());
    if (hasCloseBtn)
    {
        closeBtn = EditorWindow::AddButton(windowObject, LV_SYMBOL_CLOSE, "close");
        lv_obj_set_user_data(closeBtn, static_cast<lv_obj_user_data_t>(this));
        SetEditorEventCallback<closeCB>(closeBtn);
    }
    minimizeButton = EditorWindow::AddButton(windowObject, LV_SYMBOL_DOWN, "minimize");
    lv_obj_set_user_data(minimizeButton, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<minimizeCB>(minimizeButton);

    iconObject = lv_obj_create(parent, nullptr);
    lv_obj_set_style(iconObject, &lv_style_plain);
    lv_obj_set_size(iconObject, iconWidth, iconHeight);
    lv_obj_set_pos(iconObject, iconX, iconY);
    int titleBarSize = iconHeight / 6;
    lv_obj_t* iconTB = lv_obj_create(iconObject, nullptr);
    lv_obj_set_style(iconTB, &lv_style_plain_color);
    lv_obj_set_size(iconTB, iconWidth, titleBarSize);
    lv_obj_set_user_data(iconObject, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_user_data(iconTB, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<maximizeCB>(iconTB);
    SetEditorEventCallback<maximizeCB>(iconObject);
    lv_obj_t* minLabel = lv_label_create(iconObject, nullptr);
    lv_obj_align(minLabel, iconObject, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_width(minLabel, iconWidth);
    lv_label_set_long_mode(minLabel, LV_LABEL_LONG_SROLL);
    lv_label_set_text(minLabel, title.c_str());
    SetEditorHidden(iconObject, true);
    lv_obj_set_top(iconObject, true);
#if LVGL_VERSION_MAJOR >= 9
    if(!hasCloseBtn)EditorWindow::SetCloseHandler(windowObject,[this]{SetEditorHidden(windowObject,true);SetEditorHidden(iconObject,true);});
    EditorWindow::SetShowHandler(windowObject,[this]{SetEditorHidden(iconObject,true);isMinimized=false;});
#endif
}

lv_obj_t* MinimizableWindow::GetWindowObj()
{
    return windowObject;
}

void MinimizableWindow::closeCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    MinimizableWindow* mw = static_cast<MinimizableWindow*>(lv_obj_get_user_data(obj));
    lv_obj_del_async(mw->iconObject);
    lv_obj_del_async(mw->windowObject);
}

void MinimizableWindow::maximizeCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    MinimizableWindow* mw = static_cast<MinimizableWindow*>(lv_obj_get_user_data(obj));
    SetEditorHidden(mw->windowObject, false);
    SetEditorHidden(mw->iconObject, true);
}

void MinimizableWindow::minimizeCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    MinimizableWindow* mw = static_cast<MinimizableWindow*>(lv_obj_get_user_data(obj));
    SetEditorHidden(mw->windowObject, true);
    SetEditorHidden(mw->iconObject, false);
    lv_obj_set_top(mw->iconObject, true);
}
