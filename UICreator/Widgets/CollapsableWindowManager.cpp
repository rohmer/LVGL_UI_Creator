#include "CollapsableWindowManager.h"

std::string CollapsableWindowManager::GroupFor(lv_obj_t* object)const{
    for(auto* window:windows)for(auto* parent=object;parent;parent=lv_obj_get_parent(parent))if(parent==window->GetWindow())return window->IsVisible()?window->GetName():std::string{};
    return {};
}
bool CollapsableWindowManager::Reveal(lv_obj_t* object){
    for(auto* window:windows)for(auto* parent=object;parent;parent=lv_obj_get_parent(parent))if(parent==window->GetWindow()){
        if(!window->IsVisible())return false;if(window->IsCollapsed())window->ToggleCollapsed();Update();lv_obj_scroll_to_view_recursive(object,LV_ANIM_OFF);return true;
    }
    return false;
}
#include "../Utils/EditorStyle.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorEvents.h"

CollapsableWindowManager::CollapsableWindowManager(lv_obj_t* parent,
                                                   int x,
                                                   int y,
                                                   int verticalPadding,
                                                   bool drawSeperators) :
    parent(parent),
    x(x),
    y(y),
    verticalPadding(verticalPadding),
    seperators(drawSeperators)
{
    initialY = y;
}

void CollapsableWindowManager::SetPos(int x, int y) 
{
    this->x = x;
    this->y = y;
    this->initialY = y;
    Update();
}

void CollapsableWindowManager::AddWindow(CollapsableWindow* window)
{
    window->SetPos(x, y);
    y += window->GetCurrentHeight() + verticalPadding;
    if (seperators)
    {
        y += 3;
        lv_obj_t* s = lv_obj_create(this->parent, nullptr);
        lv_obj_set_width(s, lv_obj_get_width(this->parent) - 16);
        lv_obj_set_height(s, 5);
        lv_obj_set_x(s, 8);
        lv_obj_set_y(s, y);
        SetEditorHidden(s, false);
        lv_obj_set_style(s, &lv_style_pretty);
#if LVGL_VERSION_MAJOR >= 9
        lv_obj_set_height(s,1);
        lv_obj_set_style_bg_color(s,lv_color_hex(EditorStyle::Border),0);
        lv_obj_set_style_bg_opa(s,LV_OPA_50,0);
        lv_obj_set_style_border_width(s,0,0);
        lv_obj_set_style_radius(s,0,0);
#endif
        lv_obj_move_foreground(s);
        seps.push_back(s);
        y += 3;
    }
    windows.push_back(window);
    cbStruct* cb = new cbStruct();
    cb->cw = window;
    cb->cwm = this;
    lv_obj_set_user_data(window->GetCollapseButton(), cb);
    SetEditorEventCallback<expandCallback>(window->GetCollapseButton());
}

void CollapsableWindowManager::SetWidth(int width)
{
    for (std::vector<CollapsableWindow*>::iterator it = windows.begin();
        it != windows.end();
        ++it)
    {
        (*it)->UpdateWidth(width);
    }
    for (std::vector<lv_obj_t*>::iterator it = seps.begin();
        it != seps.end();
        ++it)
    {
        lv_obj_set_width(*it, width - 16);
    }
    Update();
}

void CollapsableWindowManager::Update()
{
    y = initialY;
    int ctr = 0;
    // We now need to reposition all our windows
    for (std::vector<CollapsableWindow*>::iterator it = windows.begin();
         it != windows.end();
         ++it)
    {
        if (!(*it)->IsVisible())
        {
            if (seperators) SetEditorHidden(seps[ctr++], true);
            continue;
        }
        (*it)->SetVerticalPos(y);
        (*it)->SetPos(x, y);
        if (seperators)
        {
            SetEditorHidden(seps[ctr], false);
            lv_obj_set_y(seps[ctr], y + (*it)->GetCurrentHeight() + verticalPadding + 3);
            lv_obj_set_x(seps[ctr], x);
            y += 6;
            ctr++;
        }
        y += (*it)->GetCurrentHeight() + verticalPadding;
    }
}

void CollapsableWindowManager::expandCallback(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    cbStruct* cb = static_cast<cbStruct *>(lv_obj_get_user_data(obj));
    cb->cw->ToggleCollapsed();

    cb->cwm->Update();
}
