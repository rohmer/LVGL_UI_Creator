#include "CommandPalette.h"
#include "EditorWindow.h"
#include "EditorControls.h"
#include "ObjectIcons.h"
#include <algorithm>
#include <cctype>
#include <memory>
namespace CommandPalette {
namespace {
struct State{lv_obj_t *root=nullptr,*query=nullptr,*list=nullptr,*count=nullptr,*oldFocus=nullptr;lv_group_t* group=nullptr;std::vector<Entry> entries;std::vector<size_t> results;std::vector<lv_obj_t*> rows;int selected=0;};
State* active=nullptr;
std::string Lower(std::string value){for(auto& c:value)c=char(std::tolower(static_cast<unsigned char>(c)));return value;}
void OldFocusDeleted(lv_event_t* event){static_cast<State*>(lv_event_get_user_data(event))->oldFocus=nullptr;}
void Choose(State* state,int index){if(state->rows.empty())return;state->selected=std::clamp(index,0,int(state->rows.size())-1);for(size_t i=0;i<state->rows.size();++i)lv_obj_set_state(state->rows[i],LV_STATE_FOCUSED,int(i)==state->selected);lv_obj_scroll_to_view(state->rows[state->selected],LV_ANIM_OFF);}
void Invoke(State* state,size_t index){if(index>=state->entries.size()||!state->entries[index].enabled)return;auto action=state->entries[index].invoke;Close();if(action)action();}
void OneLine(lv_obj_t* label){lv_obj_set_height(label,lv_font_get_line_height(lv_obj_get_style_text_font(label,LV_PART_MAIN)));lv_label_set_long_mode(label,LV_LABEL_LONG_MODE_DOTS);}
int TextWidth(lv_obj_t* label,const char* text){lv_point_t size{};lv_text_get_size(&size,text,lv_obj_get_style_text_font(label,LV_PART_MAIN),lv_obj_get_style_text_letter_space(label,LV_PART_MAIN),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);return size.x;}
void Filter(State* state){
    const auto query=Lower(lv_textarea_get_text(state->query));state->rows.clear();state->results.clear();lv_obj_clean(state->list);
    for(size_t i=0;i<state->entries.size();++i){const auto& entry=state->entries[i];if(!query.empty()&&Lower(entry.label+" "+entry.detail+" "+entry.shortcut).find(query)==std::string::npos)continue;
        state->results.push_back(i);auto* row=lv_button_create(state->list);state->rows.push_back(row);lv_obj_set_size(row,LV_PCT(100),entry.detail.empty()?38:58);lv_obj_set_click_focusable(row,false);lv_obj_set_style_shadow_width(row,0,0);lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);lv_obj_set_style_bg_color(row,lv_color_hex(0x395771),LV_STATE_FOCUSED);lv_obj_set_style_bg_opa(row,LV_OPA_COVER,LV_STATE_FOCUSED);lv_obj_set_disabled(row,!entry.enabled);
        lv_obj_set_style_pad_all(row,8,0);auto* text=lv_label_create(row);lv_label_set_text(text,entry.label.c_str());OneLine(text);int auxiliary=0;
        if(!entry.shortcut.empty()){auto* shortcut=lv_label_create(row);lv_label_set_text(shortcut,entry.shortcut.c_str());OneLine(shortcut);const int width=TextWidth(shortcut,entry.shortcut.c_str());lv_obj_set_width(shortcut,width);auxiliary=width+10;lv_obj_set_style_text_color(shortcut,lv_color_hex(EditorStyle::Muted),0);lv_obj_align(shortcut,entry.detail.empty()?LV_ALIGN_RIGHT_MID:LV_ALIGN_TOP_RIGHT,0,entry.detail.empty()?0:-3);}
        lv_obj_update_layout(row);constexpr int leading=28;const int contentWidth=lv_obj_get_content_width(row);lv_obj_set_width(text,(std::max)(1,contentWidth-leading-auxiliary));
        if(entry.detail.empty())lv_obj_align(text,LV_ALIGN_LEFT_MID,leading,0);else lv_obj_set_pos(text,leading,-3);
        if(!entry.detail.empty()){auto* detail=lv_label_create(row);lv_label_set_text(detail,entry.detail.c_str());lv_obj_set_style_text_color(detail,lv_color_hex(EditorStyle::Muted),0);lv_obj_set_width(detail,(std::max)(1,contentWidth-leading));OneLine(detail);lv_obj_set_pos(detail,leading,20);}
        if(auto* icon=ObjectIcons::Create(row,ObjectIcons::Action(entry.action.empty()?"property-search":entry.action.c_str()),20))lv_obj_align(icon,LV_ALIGN_LEFT_MID,0,0);
        struct Binding{State* state;size_t index;};lv_obj_add_event_cb(row,[](lv_event_t* event){auto* binding=static_cast<Binding*>(lv_event_get_user_data(event));if(lv_event_get_code(event)==LV_EVENT_DELETE)delete binding;else if(lv_event_get_code(event)==LV_EVENT_CLICKED)Invoke(binding->state,binding->index);},LV_EVENT_ALL,new Binding{state,i});
    }
    lv_label_set_text_fmt(state->count,"%u matches  |  Up/Down select, Enter opens, Escape closes",unsigned(state->results.size()));Choose(state,0);
}
}
void Close(){
    if(!active)return;auto* state=active;active=nullptr;
    auto* old=state->oldFocus;auto* group=state->group;if(old)lv_obj_remove_event_cb_with_user_data(old,OldFocusDeleted,state);
    EditorWindow::ReleaseFocus(state->root);lv_obj_delete(state->root);delete state;
    if(old&&group)FocusEditorControl(group,old);
}
void Open(const std::string& title,std::vector<Entry> entries,lv_group_t* group){
    Close();auto* state=new State;active=state;state->entries=std::move(entries);state->group=group;
    state->oldFocus=group?lv_group_get_focused(group):nullptr;if(state->oldFocus)lv_obj_add_event_cb(state->oldFocus,OldFocusDeleted,LV_EVENT_DELETE,state);
    state->root=EditorWindow::Create(lv_layer_top());lv_obj_set_size(state->root,(std::min)(760,lv_display_get_horizontal_resolution(nullptr)-30),(std::min)(580,lv_display_get_vertical_resolution(nullptr)-50));lv_obj_center(state->root);EditorWindow::SetTitle(state->root,title.c_str());EditorWindow::SetCloseHandler(state->root,[]{Close();});
    auto* content=EditorWindow::Content(state->root);lv_obj_set_flex_flow(content,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(content,14,0);lv_obj_set_style_pad_row(content,10,0);lv_obj_set_scrollable(content,false);
    state->query=lv_textarea_create(content);lv_obj_set_size(state->query,LV_PCT(100),40);lv_textarea_set_one_line(state->query,true);lv_textarea_set_placeholder_text(state->query,"Type to search...");
    state->count=lv_label_create(content);lv_obj_set_style_text_color(state->count,lv_color_hex(EditorStyle::Muted),0);
    state->list=lv_obj_create(content);lv_obj_set_width(state->list,LV_PCT(100));lv_obj_set_flex_grow(state->list,1);lv_obj_set_flex_flow(state->list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(state->list,3,0);lv_obj_set_style_pad_row(state->list,3,0);
    lv_obj_add_event_cb(state->query,[](lv_event_t* event){Filter(static_cast<State*>(lv_event_get_user_data(event)));},LV_EVENT_VALUE_CHANGED,state);
    Filter(state);if(group)FocusEditorControl(group,state->query);
}
bool HandleEvent(const SDL_Event& event){
    if(!active||event.type!=SDL_EVENT_KEY_DOWN)return false;auto* state=active;
    switch(event.key.key){case SDLK_ESCAPE:Close();return true;case SDLK_UP:Choose(state,state->selected-1);return true;case SDLK_DOWN:Choose(state,state->selected+1);return true;case SDLK_RETURN:case SDLK_KP_ENTER:if(!state->results.empty())Invoke(state,state->results[state->selected]);return true;default:return false;}
}
lv_obj_t* ActiveWindow(){return active?active->root:nullptr;}
size_t ResultCount(){return active?active->results.size():0;}
}
