#include "CollapsableWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorEvents.h"
#include "../Utils/EditorStyle.h"

CollapsableWindow::CollapsableWindow(lv_obj_t* parent,
                                     std::string name,
                                     bool isCollapsed,
                                     int x,
                                     int y,
                                     int width,
                                     int height) :
    name(name),
    isCollapsed(isCollapsed),
    x(x),
    y(y),
    width(width),
    height(height)
{
    createObjects(parent);
}

CollapsableWindow::CollapsableWindow(lv_obj_t* parent,
                                     std::string name,
                                     bool isCollapsed,
                                     int width,
                                     int height) :
    name(name),
    isCollapsed(isCollapsed),
    x(0),
    y(0),
    width(width),
    height(height)
{
    createObjects(parent);
}

void CollapsableWindow::UpdateHeight(int height)
{
    this->height = height;
    lv_obj_set_height(window, height);
}

void CollapsableWindow::UpdateWidth(int width)
{
    lv_obj_set_width(window, width);
}

void CollapsableWindow::SetVisible(bool show)
{
    visible = show;
    SetEditorHidden(winTitle, !show);
    SetEditorHidden(collapseBtn, !show);
    SetEditorHidden(window, !show || isCollapsed);
}

const std::string CollapsableWindow::GetName()
{
    return name;
}

void CollapsableWindow::createObjects(lv_obj_t* parent)
{
    if (parent == nullptr)
    {
        window = lv_cont_create(lv_scr_act(), nullptr);
    }
    else
    {
        window = lv_cont_create(parent, nullptr);
    }
    lv_obj_set_size(window, width, height);

    collapseBtn = lv_btn_create(parent, nullptr);
    collapseBtnLbl = lv_label_create(collapseBtn, nullptr);
    lv_obj_set_size(collapseBtn, 15, 15);
    winTitle = lv_label_create(parent, nullptr);
    lv_label_set_text(winTitle, name.c_str());
    lv_obj_set_pos(winTitle, x + 25, 7);
    lv_obj_set_pos(collapseBtn, x + 5, y + 7);
    if (isCollapsed)
    {
        lv_label_set_text(collapseBtnLbl, LV_SYMBOL_RIGHT);
    }
    else
    {
        lv_label_set_text(collapseBtnLbl, LV_SYMBOL_DOWN);
    }
    lv_cont_set_layout(window, LV_LAYOUT_OFF);
    lv_cont_set_fit2(window, LV_FIT_NONE, LV_FIT_NONE);
    lv_obj_set_pos(window, x, y + 25);

    lv_obj_set_style(window, &lv_style_transp);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_width(window, LV_PCT(100));
    lv_obj_set_style_max_width(window, width, LV_PART_MAIN);
    lv_obj_set_height(window, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(window, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(window, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(window, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(window, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(collapseBtn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(collapseBtn, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(collapseBtn, 0, LV_PART_MAIN);
    lv_obj_set_size(collapseBtn, 22, 22);
    lv_obj_set_style_text_color(winTitle, lv_color_hex(EditorStyle::Muted), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(winTitle, 1, LV_PART_MAIN);
    lv_obj_center(collapseBtnLbl);
    lv_obj_set_hidden(window, isCollapsed);
#endif
    cbStruct* userData = new cbStruct();
    userData->cw = this;
    lv_obj_set_user_data(collapseBtn, userData);
    SetEditorEventCallback<collapseClicked>(collapseBtn);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_clickable(winTitle,true);
    lv_obj_add_event_cb(winTitle,[](lv_event_t* event) {
        auto* panel=static_cast<CollapsableWindow*>(lv_event_get_user_data(event));
        lv_obj_send_event(panel->GetCollapseButton(),LV_EVENT_CLICKED,nullptr);
    },LV_EVENT_CLICKED,this);
#endif
}

void CollapsableWindow::SetPos(int x, int y)
{
    this->x = x;
    this->y = y;
    lv_obj_set_pos(winTitle, x + 33, 7 + y);
    lv_obj_set_pos(collapseBtn, x + 5, 7 + y);
    lv_obj_set_pos(window, x, y + 25);
}

void CollapsableWindow::SetVerticalPos(int y)
{
    this->y = y;
    lv_obj_set_pos(winTitle, this->x + 33, 7 + y);
    lv_obj_set_pos(collapseBtn, this->x + 5, 7 + y);
    lv_obj_set_pos(window, this->x, y + 25);
}

void CollapsableWindow::ToggleCollapsed()
{
#if LVGL_VERSION_MAJOR >= 9
    isCollapsed = !isCollapsed;
    lv_label_set_text(collapseBtnLbl, isCollapsed ? LV_SYMBOL_RIGHT : LV_SYMBOL_DOWN);
    lv_obj_set_hidden(window, isCollapsed);
    return;
#endif
    if (isCollapsed)
        isCollapsed = false;
    else
        isCollapsed = true;
    if (isCollapsed)
    {
        lv_label_set_text(collapseBtnLbl, LV_SYMBOL_RIGHT);
        lv_cont_set_fit(window, LV_FIT_NONE);
        lv_obj_set_size(window, width, 0);
    }
    else
    {
        lv_label_set_text(collapseBtnLbl, LV_SYMBOL_DOWN);
        lv_obj_set_size(window, width, height);
        lv_cont_set_fit(window, LV_FIT_TIGHT);
    }
}

void CollapsableWindow::collapseClicked(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    cbStruct* cb = static_cast<cbStruct*>(lv_obj_get_user_data(obj));
    CollapsableWindow* cw = cb->cw;
    if (cw->isCollapsed)
    {
        lv_label_set_text(cw->collapseBtnLbl, LV_SYMBOL_DOWN);
        cw->isCollapsed = false;
        lv_obj_set_size(cw->window, cw->width, cw->height);
        lv_cont_set_layout(cw->window, LV_FIT_TIGHT);
    }
    else
    {
        lv_label_set_text(cw->collapseBtnLbl, LV_SYMBOL_RIGHT);
        cw->isCollapsed = true;
        // WE need to hide everything in the window
        lv_cont_set_layout(cw->window, LV_FIT_NONE);
        lv_obj_set_size(cw->window, cw->width, 0);
    }
}

int CollapsableWindow::GetCurrentHeight()
{
    if (!visible) return 0;
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_update_layout(window);
    return 32 + (isCollapsed ? 0 : lv_obj_get_height(window));
#endif
    std::vector<lv_obj_t*> children = ObjectTools::GetChildren(window);
    return (lv_obj_get_height(window));
}

lv_obj_t* CollapsableWindow::GetWindow()
{
    return window;
}

void CollapsableWindow::DeleteObjects()
{
    for (int i = 0; i < objects.size(); i++)
        lv_obj_del_async(objects[i]);
}

void CollapsableWindow::AddObjectToWindow(lv_obj_t* obj)
{
    lv_obj_set_parent(obj, window);
#if LVGL_VERSION_MAJOR < 9
    lv_obj_set_y(obj, lv_obj_get_y(obj) + 10);
#endif
    objects.push_back(obj);
}

lv_obj_t* CollapsableWindow::GetCollapseButton()
{
    return collapseBtn;
}

void CollapsableWindow::DeleteChildren()
{
    for (std::vector<lv_obj_t*>::iterator it = objects.begin();
         it != objects.end();
         ++it)
    {
        lv_obj_delete(*it);
    }
    objects.clear();
}
