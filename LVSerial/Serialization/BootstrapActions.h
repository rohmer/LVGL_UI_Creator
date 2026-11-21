#pragma once
#include <lvgl/lvgl.h>
#include <string>

// Portable behavior for recipe trees. Resolve relatives at click time so no
// callback retains pointers to deleted siblings, and cloning remains safe.
namespace Serialization::BootstrapActions
{
    inline void Dismiss(lv_event_t* event)
    {
        auto* source = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
        if(auto* parent = lv_obj_get_parent(source)) lv_obj_set_hidden(parent, true);
    }
    inline void Collapse(lv_event_t* event)
    {
        auto* source = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
        if(auto* parent = lv_obj_get_parent(source))
            if(auto* content = lv_obj_get_child(parent, 1)) lv_obj_set_hidden(content, !lv_obj_is_hidden(content));
    }
    inline void Bind(lv_obj_t* object, const std::string& action)
    {
        lv_obj_remove_event_cb(object, Dismiss);
        lv_obj_remove_event_cb(object, Collapse);
        if(action == "dismiss") lv_obj_add_event_cb(object, Dismiss, LV_EVENT_CLICKED, nullptr);
        else if(action == "collapse") lv_obj_add_event_cb(object, Collapse, LV_EVENT_CLICKED, nullptr);
    }
}
