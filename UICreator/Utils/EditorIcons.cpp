#include "EditorIcons.h"
#include "ObjectIcons.h"
#include "EditorStyle.h"
#include <algorithm>
#include <string>

namespace EditorIcons {
namespace {
struct Decoration {
    lv_obj_t *button, *label, *icon, *tooltip = nullptr;
    std::string action;
    bool iconOnly;
    int size;
};
void Watch(lv_event_t* event);
Decoration* Find(lv_obj_t* button) {
    if(!button)return nullptr;
    for(uint32_t index=0;index<lv_obj_get_event_count(button);++index) {
        auto* event=lv_obj_get_event_dsc(button,index);
        if(lv_event_dsc_get_cb(event)==Watch)
            return static_cast<Decoration*>(lv_event_dsc_get_user_data(event));
    }
    return nullptr;
}
void TooltipDeleted(lv_event_t* event) {
    static_cast<Decoration*>(lv_event_get_user_data(event))->tooltip=nullptr;
}
void HideHint(Decoration* state) {
    if(!state->tooltip)return;
    auto* tooltip=state->tooltip;state->tooltip=nullptr;
    lv_obj_remove_event_cb_with_user_data(tooltip,TooltipDeleted,state);
    lv_obj_delete(tooltip);
}
void Layout(Decoration* state) {
    if(state->iconOnly) {
        lv_obj_set_hidden(state->label,true);
        lv_obj_center(state->icon);
    } else {
        lv_obj_set_hidden(state->label,false);
        const int left=state->size+8;
        lv_obj_set_width(state->label,(std::max)(1,int(lv_obj_get_content_width(state->button))-left));
        lv_obj_set_height(state->label,lv_font_get_line_height(lv_obj_get_style_text_font(state->label,LV_PART_MAIN)));
        lv_label_set_long_mode(state->label,LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(state->icon,LV_ALIGN_LEFT_MID,0,0);
        lv_obj_align(state->label,LV_ALIGN_LEFT_MID,left,0);
    }
}
void ShowHint(Decoration* state) {
    if(!state->iconOnly || state->tooltip || lv_obj_is_disabled(state->button))return;
    const auto* descriptor=ObjectIcons::Get(state->icon);
    if(!descriptor)return;
    auto* display=lv_obj_get_display(state->button);
    auto* tooltip=lv_obj_create(lv_display_get_layer_sys(display));state->tooltip=tooltip;
    lv_obj_add_event_cb(tooltip,TooltipDeleted,LV_EVENT_DELETE,state);
    lv_obj_set_size(tooltip,LV_SIZE_CONTENT,LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(tooltip,7,0);
    lv_obj_set_style_border_width(tooltip,1,0);
    lv_obj_set_style_border_color(tooltip,lv_color_hex(EditorStyle::Border),0);
    lv_obj_set_style_radius(tooltip,5,0);
    lv_obj_set_style_bg_color(tooltip,lv_color_hex(EditorStyle::Input),0);
    lv_obj_set_style_bg_opa(tooltip,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(tooltip,lv_color_hex(EditorStyle::Text),0);
    lv_obj_set_clickable(tooltip,false);
    lv_obj_set_click_focusable(tooltip,false);
    lv_obj_set_scrollable(tooltip,false);
    auto* label=lv_label_create(tooltip);lv_label_set_text(label,descriptor->name);
    lv_obj_set_clickable(label,false);
    lv_obj_set_click_focusable(label,false);
    lv_obj_update_layout(tooltip);
    lv_area_t anchor{};lv_obj_get_coords(state->button,&anchor);
    const int width=lv_obj_get_width(tooltip),height=lv_obj_get_height(tooltip);
    const int displayWidth=lv_display_get_horizontal_resolution(display),displayHeight=lv_display_get_vertical_resolution(display);
    const int x=std::clamp((anchor.x1+anchor.x2-width)/2,4,(std::max)(4,displayWidth-width-4));
    int y=anchor.y2+6;if(y+height>displayHeight-4)y=anchor.y1-height-6;
    lv_obj_set_pos(tooltip,x,(std::max)(4,y));
}
void Watch(lv_event_t* event) {
    auto* state=static_cast<Decoration*>(lv_event_get_user_data(event));
    switch(lv_event_get_code(event)) {
    case LV_EVENT_DELETE:HideHint(state);delete state;break;
    case LV_EVENT_SIZE_CHANGED:
    case LV_EVENT_STYLE_CHANGED:Layout(state);break;
    case LV_EVENT_HOVER_OVER:ShowHint(state);break;
    case LV_EVENT_HOVER_LEAVE:
    case LV_EVENT_PRESSED:
    case LV_EVENT_DEFOCUSED:HideHint(state);break;
    case LV_EVENT_STATE_CHANGED:if(lv_obj_is_disabled(state->button))HideHint(state);break;
    default:break;
    }
}
}
bool Set(lv_obj_t* button,const char* action,bool iconOnly,int size) {
    const auto* descriptor=ObjectIcons::Action(action);
    if(!button || !descriptor || size<=0)return false;
    auto* state=Find(button);
    if(!state) {
        lv_obj_t* label=nullptr;
        for(uint32_t index=0;index<lv_obj_get_child_count(button);++index) {
            auto* child=lv_obj_get_child(button,index);
            if(lv_obj_has_class(child,&lv_label_class)){label=child;break;}
        }
        if(!label)label=lv_label_create(button);
        auto* icon=ObjectIcons::Create(button,descriptor,size);
        state=new Decoration{button,label,icon,nullptr,action,iconOnly,size};
        lv_obj_add_event_cb(button,Watch,LV_EVENT_ALL,state);
        lv_obj_set_style_bg_color(button,lv_color_hex(0x29445a),LV_PART_MAIN|LV_STATE_PRESSED);
        lv_obj_set_style_opa(button,LV_OPA_40,LV_PART_MAIN|LV_STATE_DISABLED);
    } else {
        if(state->action!=action || state->iconOnly!=iconOnly)HideHint(state);
        state->action=action;state->iconOnly=iconOnly;state->size=size;
        ObjectIcons::Set(state->icon,descriptor);
        lv_obj_set_size(state->icon,size,size);
    }
    Layout(state);
    return true;
}
lv_obj_t* Icon(lv_obj_t* button) {const auto* state=Find(button);return state?state->icon:nullptr;}
const char* Action(lv_obj_t* button) {const auto* state=Find(button);return state?state->action.c_str():nullptr;}
const char* Hint(lv_obj_t* button) {const auto* icon=ObjectIcons::Get(Icon(button));return icon?icon->name:nullptr;}
}
