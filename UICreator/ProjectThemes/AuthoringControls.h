#pragma once
#include "DesignContext.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorSession.h"
#include <functional>

namespace ProjectAuthoring {
inline lv_obj_t* Row(lv_obj_t* parent)
{
    auto* row=lv_obj_create(parent);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_all(row,0,0);
    lv_obj_set_style_border_width(row,0,0);lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);
    lv_obj_set_style_pad_column(row,8,0);lv_obj_set_style_pad_row(row,8,0);lv_obj_set_scrollable(row,false);return row;
}
inline void Focus(lv_obj_t* field,lv_group_t* group)
{
    if(!group)return;
    lv_obj_add_event_cb(field,[](lv_event_t* e){FocusEditorControl(static_cast<lv_group_t*>(lv_event_get_user_data(e)),lv_event_get_target_obj(e));},LV_EVENT_CLICKED,group);
}
inline lv_obj_t* Button(lv_obj_t* parent,const char* text,std::function<void()> action,int width=150,lv_group_t* group=nullptr)
{
    auto* b=lv_button_create(parent);lv_obj_set_size(b,width,36);EditorStyle::Button(b);
    auto* label=lv_label_create(b);lv_label_set_text(label,text);lv_obj_center(label);
    lv_obj_add_event_cb(b,[](lv_event_t* e){auto* fn=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete fn;else if(lv_event_get_code(e)==LV_EVENT_CLICKED){auto action=*fn;action();}},LV_EVENT_ALL,new std::function<void()>(std::move(action)));Focus(b,group);return b;
}
inline lv_obj_t* Field(lv_obj_t* parent,const char* placeholder,lv_group_t* group)
{
    auto* field=lv_textarea_create(parent);lv_textarea_set_one_line(field,true);lv_textarea_set_placeholder_text(field,placeholder);
    lv_obj_set_size(field,LV_PCT(100),38);EditorStyle::Control(field);Focus(field,group);return field;
}
inline void Column(lv_obj_t* object)
{
    lv_obj_set_flex_flow(object,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(object,14,0);
    lv_obj_set_style_pad_row(object,12,0);lv_obj_set_style_border_width(object,0,0);
    lv_obj_set_style_radius(object,0,0);lv_obj_set_style_bg_color(object,lv_color_hex(EditorStyle::Surface),0);
}
}
