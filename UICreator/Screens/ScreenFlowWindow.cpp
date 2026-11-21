#include "ScreenFlowWindow.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include <Serialization/ProjectScreens.h>
#include <algorithm>
namespace ScreenFlowWindow {
namespace {
namespace S=Serialization::ProjectScreens;using json=nlohmann::json;
struct Window{Context context;lv_obj_t *root=nullptr,*cards=nullptr,*name=nullptr,*transition=nullptr,*duration=nullptr,*widgets=nullptr,*target=nullptr,*status=nullptr;std::vector<int> oids;bool deleting=false;size_t deleteScreen=0;};
Window* active=nullptr;
const char* types[]={"none","fade","slide_left","slide_right","slide_up","slide_down"};
lv_obj_t* Label(lv_obj_t* parent,const std::string& text){auto* label=lv_label_create(parent);lv_label_set_text(label,text.c_str());lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);return label;}
void Focus(Window* w,lv_obj_t* object){lv_obj_add_event_cb(object,[](lv_event_t* event){auto* w=static_cast<Window*>(lv_event_get_user_data(event));FocusEditorControl(w->context.keyboardGroup,lv_event_get_target_obj(event));},LV_EVENT_CLICKED,w);}
lv_obj_t* Button(Window* w,lv_obj_t* parent,const char* text,std::function<void()> action){auto* button=lv_button_create(parent);lv_obj_set_size(button,LV_PCT(100),36);lv_obj_set_style_shadow_width(button,0,0);auto* label=lv_label_create(button);lv_label_set_text(label,text);lv_obj_center(label);lv_obj_add_event_cb(button,[](lv_event_t* e){auto* function=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete function;else if(lv_event_get_code(e)==LV_EVENT_CLICKED){auto action=*function;action();}},LV_EVENT_ALL,new std::function<void()>(std::move(action)));Focus(w,button);return button;}
void Refresh(Window* w){
    auto doc=w->context.document();S::EnsureMulti(doc);const auto current=S::Editing(doc);w->deleting=false;lv_obj_clean(w->cards);std::string targets;
    for(size_t i=0;i<S::Count(doc);++i){const auto& screen=doc["screens"][i];const auto name=screen.value("name",std::string("Screen"));if(i)targets+='\n';targets+=name;
        auto* card=lv_obj_create(w->cards);lv_obj_set_size(card,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(card,10,0);lv_obj_set_style_pad_row(card,6,0);lv_obj_set_style_border_color(card,lv_color_hex(i==current?0x58b8e8:0x4b5664),0);
        const auto caption=(i==doc.value("activeScreen",size_t(0))?"[Start] ":"")+name;
        Button(w,card,caption.c_str(),[w,i]{w->context.selectScreen(i);Refresh(w);});
        Label(card,std::to_string(screen["obj"].size())+" widgets");
        for(const auto& record:screen["obj"])for(const auto& event:record.value("base",json::object()).value("events",json::array()))if(event.value("action",std::string{})=="navigate"){
            const auto target=event.value("screen",size_t(-1));const auto targetName=target<S::Count(doc)?doc["screens"][target].value("name",std::string("Screen")):"Missing screen";
            Label(card,record.value("base",json::object()).value("name",std::string("Widget"))+" -> "+targetName);
        }
    }
    const auto& screen=doc["screens"][current];lv_textarea_set_text(w->name,screen.value("name",std::string("Screen")).c_str());
    const auto transition=screen.value("transition",json::object());unsigned selected=0;for(unsigned i=0;i<6;++i)if(transition.value("type",std::string("none"))==types[i])selected=i;
    lv_dropdown_set_selected(w->transition,selected);lv_spinbox_set_value(w->duration,transition.value("duration",0));
    w->oids.clear();std::string widgets;for(const auto& record:screen["obj"]){if(!widgets.empty())widgets+='\n';widgets+=record.value("base",json::object()).value("name",std::string("Widget ")+std::to_string(record.value("oid",0)));w->oids.push_back(record.value("oid",0));}
    lv_dropdown_set_options(w->widgets,widgets.empty()?"No widgets":widgets.c_str());lv_dropdown_set_options(w->target,targets.c_str());
}
template<class F>void Edit(Window* w,F action,const char* message){try{auto doc=w->context.document();S::EnsureMulti(doc);action(doc);const auto context=w->context;context.apply(std::move(doc));if(active==w){Refresh(w);lv_label_set_text(w->status,message);}}catch(const std::exception& error){if(active==w)lv_label_set_text(w->status,error.what());}}
}
void Close(){if(!active)return;auto* w=active;active=nullptr;EditorWindow::ReleaseFocus(w->root);lv_obj_delete(w->root);delete w;}
void Refresh(){if(active)Refresh(active);}
lv_obj_t* ActiveWindow(){return active?active->root:nullptr;}
void Open(Context context){
    Close();auto* w=new Window;active=w;w->context=std::move(context);w->root=EditorWindow::Create(lv_layer_top());lv_obj_set_size(w->root,(std::min)(1000,lv_display_get_horizontal_resolution(nullptr)-30),(std::min)(820,lv_display_get_vertical_resolution(nullptr)-50));lv_obj_center(w->root);EditorWindow::SetTitle(w->root,"Screens and navigation");EditorWindow::SetCloseHandler(w->root,[]{Close();});
    auto* body=EditorWindow::Content(w->root);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_ROW);lv_obj_set_scrollable(body,false);lv_obj_set_style_pad_all(body,12,0);lv_obj_set_style_pad_column(body,12,0);
    w->cards=lv_obj_create(body);lv_obj_set_size(w->cards,LV_PCT(45),LV_PCT(100));lv_obj_set_flex_flow(w->cards,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(w->cards,4,0);lv_obj_set_style_pad_row(w->cards,8,0);
    auto* tools=lv_obj_create(body);lv_obj_set_size(tools,LV_PCT(53),LV_PCT(100));lv_obj_set_flex_flow(tools,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(tools,8,0);lv_obj_set_style_pad_row(tools,8,0);
    Label(tools,"Screen name");w->name=lv_textarea_create(tools);lv_textarea_set_one_line(w->name,true);lv_obj_set_size(w->name,LV_PCT(100),38);Focus(w,w->name);
    Label(tools,"Entry transition");w->transition=lv_dropdown_create(tools);lv_obj_set_width(w->transition,LV_PCT(100));lv_dropdown_set_options(w->transition,"None\nFade\nSlide left\nSlide right\nSlide up\nSlide down");Focus(w,w->transition);
    Label(tools,"Transition duration (ms)");w->duration=lv_spinbox_create(tools);lv_obj_set_size(w->duration,LV_PCT(100),38);lv_spinbox_set_range(w->duration,0,2000);lv_spinbox_set_digit_format(w->duration,4,0);Focus(w,w->duration);
    Button(w,tools,"Save screen properties",[w]{const auto name=std::string(lv_textarea_get_text(w->name));const auto type=types[lv_dropdown_get_selected(w->transition)];const int duration=lv_spinbox_get_value(w->duration);Edit(w,[&](json& doc){const auto index=S::Editing(doc);S::Rename(doc,index,name);doc["screens"][index]["transition"]={{"type",type},{"duration",duration}};},"Screen properties saved. Undo is available.");});
    Button(w,tools,"Add screen",[w]{Edit(w,[](json& doc){S::Add(doc,"Screen "+std::to_string(S::Count(doc)+1));},"Screen added.");});
    Button(w,tools,"Duplicate selected screen",[w]{Edit(w,[](json& doc){S::Duplicate(doc,S::Editing(doc));},"Screen duplicated with independent local variable bindings.");});
    Button(w,tools,"Move screen up",[w]{Edit(w,[](json& doc){auto i=S::Editing(doc);if(i)S::Move(doc,i,i-1);},"Screen order and navigation references updated.");});
    Button(w,tools,"Move screen down",[w]{Edit(w,[](json& doc){auto i=S::Editing(doc);if(i+1<S::Count(doc))S::Move(doc,i,i+1);},"Screen order and navigation references updated.");});
    Button(w,tools,"Use as start screen",[w]{Edit(w,[](json& doc){S::SetStart(doc,S::Editing(doc));},"Application start screen updated.");});
    Button(w,tools,"Delete selected screen",[w]{auto doc=w->context.document();const auto index=S::Editing(doc);if(!w->deleting||w->deleteScreen!=index){w->deleting=true;w->deleteScreen=index;lv_label_set_text(w->status,"Click Delete selected screen again to confirm. Navigation links to it will be removed and its variable bindings cleared.");return;}Edit(w,[&](json& d){S::Remove(d,index);},"Screen deleted. Undo restores the screen and its links.");});
    Label(tools,"Add click navigation from widget");w->widgets=lv_dropdown_create(tools);lv_obj_set_width(w->widgets,LV_PCT(100));Focus(w,w->widgets);Label(tools,"Destination screen");w->target=lv_dropdown_create(tools);lv_obj_set_width(w->target,LV_PCT(100));Focus(w,w->target);
    Button(w,tools,"Add navigation link",[w]{const auto selected=lv_dropdown_get_selected(w->widgets);if(selected>=w->oids.size()){lv_label_set_text(w->status,"Select a screen with a widget first.");return;}const auto oid=w->oids[selected];const auto target=lv_dropdown_get_selected(w->target);Edit(w,[&](json& doc){for(auto& record:doc["obj"])if(record.value("oid",-1)==oid){auto& events=record["base"]["events"];if(!events.is_array())events=json::array();events.push_back({{"event","clicked"},{"action","navigate"},{"screen",target}});}S::SyncAlias(doc);},"Navigation added. Configure conditions and other events in Edit widget events.");});
    Button(w,tools,"Run application from start",[w]{const auto run=w->context.run;Close();if(run)run();});
    w->status=Label(tools,"Select a screen to edit it. Reordering preserves navigation and bindings. Run mode supports Back and entry transitions.");Refresh(w);
}
}
