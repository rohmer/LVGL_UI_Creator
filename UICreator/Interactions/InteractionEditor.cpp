#include "InteractionEditor.h"
#include "../Windows/PropertyWindow.h"
#include "../Widgets/TreeView.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorEdit.h"
#include "../../LVSerial/Serialization/InteractionActions.h"
#include "../../LVSerial/Serialization/ObjectUserData.h"
#include "../../LVSerial/Serialization/LVVariable.h"
#include <algorithm>
#include <map>
#include <unordered_map>

namespace InteractionEditor {
namespace {
using json=nlohmann::json;
struct Window {
    PropertyWindow* properties; std::function<json()> document;
    lv_obj_t *root,*sourceLabel,*list,*event,*action,*target,*screen,*variable,*value,*required,*minimum,*maximum,*conditionVariable,*conditionOp,*conditionValue,*status;
    lv_obj_t* source=nullptr; lv_timer_t* timer=nullptr;
    std::vector<lv_obj_t*> objects; std::vector<std::string> variables; json actions=json::array();
};
Window* active=nullptr;
constexpr const char* events[]={"clicked","value_changed","ready"};
constexpr const char* actions[]={"navigate","set_variable","increment_variable","show","hide","toggle","validate"};
constexpr const char* operators[]={"eq","ne","gt","lt","truthy"};
std::string Name(lv_obj_t* object,size_t index) {auto* d=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));auto name=d?d->objectName:std::string{};std::replace(name.begin(),name.end(),'\n',' ');return std::to_string(index)+": "+(name.empty()?"Widget":name);}
lv_obj_t* Label(lv_obj_t* body,const char* text) {auto* l=lv_label_create(body);lv_obj_set_width(l,LV_PCT(100));lv_label_set_text(l,text);lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);return l;}
lv_obj_t* Select(Window* w,lv_obj_t* body,const char* title,const char* options) {Label(body,title);auto* d=lv_dropdown_create(body);lv_obj_set_width(d,LV_PCT(100));lv_dropdown_set_options(d,options);BindEditorControl(w->properties->GetKBGroup(),d);return d;}
lv_obj_t* Input(Window* w,lv_obj_t* body,const char* title,const char* text="") {Label(body,title);auto* input=lv_textarea_create(body);lv_obj_set_width(input,LV_PCT(100));lv_textarea_set_one_line(input,true);lv_textarea_set_text(input,text);lv_obj_add_event_cb(input,[](lv_event_t* e){auto* w=static_cast<Window*>(lv_event_get_user_data(e));FocusEditorControl(w->properties->GetKBGroup(),lv_event_get_target_obj(e));},LV_EVENT_CLICKED,w);return input;}
void Button(lv_obj_t* parent,const char* text,std::function<void()> fn) {auto* b=lv_button_create(parent);lv_obj_set_size(b,LV_PCT(100),36);auto* l=lv_label_create(b);lv_label_set_text(l,text);lv_obj_center(l);lv_obj_add_event_cb(b,[](lv_event_t* e){auto* f=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete f;else if(lv_event_get_code(e)==LV_EVENT_CLICKED)(*f)();},LV_EVENT_ALL,new std::function<void()>(std::move(fn)));}
template<size_t N> uint32_t Index(const char* const(&values)[N],const std::string& value) {for(uint32_t i=0;i<N;++i)if(value==values[i])return i;return 0;}
std::string JsonText(const json& value) {return value.dump();}
json Parse(const char* text) {auto result=json::parse(text,nullptr,false);return result.is_discarded()?json(text):result;}
void LoadAction(Window* w) {
    const auto index=lv_dropdown_get_selected(w->list);if(index>=w->actions.size())return;const auto& a=w->actions[index];
    lv_dropdown_set_selected(w->event,Index(events,a.value("event",std::string("clicked"))));lv_dropdown_set_selected(w->action,Index(actions,a.value("action",std::string("toggle"))));
    const int target=a.value("targetOid",-1);lv_dropdown_set_selected(w->target,target>=0&&size_t(target)<w->objects.size()?target+1:0);
    lv_textarea_set_text(w->screen,std::to_string(a.value("screen",0)+1).c_str());
    auto selectVariable=[&](lv_obj_t* dropdown,const std::string& name,bool optional){auto i=std::find(w->variables.begin(),w->variables.end(),name);lv_dropdown_set_selected(dropdown,i==w->variables.end()?0:uint32_t(i-w->variables.begin())+(optional?1:0));};
    selectVariable(w->variable,a.value("variable",std::string{}),false);lv_textarea_set_text(w->value,JsonText(a.value("value",json(1))).c_str());
    if(a.value("required",true))lv_obj_add_state(w->required,LV_STATE_CHECKED);else lv_obj_remove_state(w->required,LV_STATE_CHECKED);
    lv_textarea_set_text(w->minimum,a.contains("min")?a.at("min").dump().c_str():"");lv_textarea_set_text(w->maximum,a.contains("max")?a.at("max").dump().c_str():"");
    const auto c=a.value("condition",json::object());selectVariable(w->conditionVariable,c.value("variable",std::string{}),true);lv_dropdown_set_selected(w->conditionOp,Index(operators,c.value("op",std::string("eq"))));lv_textarea_set_text(w->conditionValue,JsonText(c.value("value",json(true))).c_str());
}
void RefreshList(Window* w,uint32_t selected=0) {std::string options;for(size_t i=0;i<w->actions.size();++i){if(i)options+='\n';options+=std::to_string(i+1)+". "+w->actions[i].value("event",std::string{})+" -> "+w->actions[i].value("action",std::string{});}lv_dropdown_set_options(w->list,options.empty()?"(No actions)":options.c_str());lv_dropdown_set_selected(w->list,std::min(selected,uint32_t(w->actions.empty()?0:w->actions.size()-1)));LoadAction(w);}
void Refresh(Window* w) {
    w->objects=w->properties->GetObjectTree()->GetObjects();w->source=w->properties->GetSelectedObject();
    auto found=std::find(w->objects.begin(),w->objects.end(),w->source);
    if(found==w->objects.end()){w->source=nullptr;w->actions=json::array();lv_label_set_text(w->sourceLabel,"Select a widget in the object tree.");RefreshList(w);return;}
    lv_label_set_text(w->sourceLabel,("Selected widget: "+Name(w->source,size_t(found-w->objects.begin()))).c_str());
    std::string targets="This widget";std::unordered_map<lv_obj_t*,int> indices;for(size_t i=0;i<w->objects.size();++i){targets+='\n'+Name(w->objects[i],i);indices[w->objects[i]]=int(i);}lv_dropdown_set_options(w->target,targets.c_str());
    try {w->actions=Serialization::InteractionActions::EventsForSave(w->source,indices);auto doc=w->document();w->variables.clear();std::string options;for(const auto& v:doc.value("variables",json::array())) {auto name=Serialization::LVVariable::FromJSON(v).name;if(name.empty())continue;w->variables.push_back(name);if(!options.empty())options+='\n';options+=name;}lv_dropdown_set_options(w->variable,options.empty()?"(No variables)":options.c_str());lv_dropdown_set_options(w->conditionVariable,("Always"+(options.empty()?std::string{}:"\n"+options)).c_str());RefreshList(w);}
    catch(const std::exception& e){lv_label_set_text(w->status,e.what());}
}
void Store(Window* w,uint32_t selected) {EditorEdit::Scope edit;std::map<int,lv_obj_t*> objects;for(size_t i=0;i<w->objects.size();++i)objects.emplace(int(i),w->objects[i]);Serialization::InteractionActions::SetEvents(w->source,w->actions,objects);RefreshList(w,selected);lv_label_set_text(w->status,"Saved. Use Run mode or Debug screen to try the actions.");}
void Save(Window* w) {
    try {
        const auto selected=lv_dropdown_get_selected(w->list);if(!w->source||selected>=w->actions.size())throw std::invalid_argument("Add an action first");
        json a={{"event",events[lv_dropdown_get_selected(w->event)]},{"action",actions[lv_dropdown_get_selected(w->action)]}};const auto action=a.at("action").get<std::string>();
        if(action=="navigate"){int screen=std::stoi(lv_textarea_get_text(w->screen));auto doc=w->document();size_t count=doc.contains("screens")?doc["screens"].size():1;if(screen<1||size_t(screen)>count)throw std::invalid_argument("Screen number is outside this project");a["screen"]=screen-1;}
        else if(action=="set_variable"||action=="increment_variable"){auto i=lv_dropdown_get_selected(w->variable);if(i>=w->variables.size())throw std::invalid_argument("Create a project variable first");a["variable"]=w->variables[i];a["value"]=Parse(lv_textarea_get_text(w->value));}
        else {a["targetOid"]=int(lv_dropdown_get_selected(w->target))-1;if(action=="validate"){a["required"]=lv_obj_has_state(w->required,LV_STATE_CHECKED);for(auto field:{std::make_pair("min",w->minimum),std::make_pair("max",w->maximum)})if(*lv_textarea_get_text(field.second))a[field.first]=Parse(lv_textarea_get_text(field.second));}}
        auto condition=lv_dropdown_get_selected(w->conditionVariable);if(condition&&condition<=w->variables.size())a["condition"]={{"variable",w->variables[condition-1]},{"op",operators[lv_dropdown_get_selected(w->conditionOp)]},{"value",Parse(lv_textarea_get_text(w->conditionValue))}};
        auto normalized=Serialization::InteractionActions::Normalize(json::array({a}));w->actions[selected]=normalized.front();Store(w,selected);
    }catch(const std::exception& e){lv_label_set_text(w->status,e.what());}
}
}
void Close(){if(active)lv_obj_delete(active->root);}
void Open(PropertyWindow* properties,std::function<json()> document) {
    if(active){lv_obj_move_foreground(active->root);Refresh(active);return;}
    auto* w=new Window{};w->properties=properties;w->document=std::move(document);w->root=EditorWindow::Create(lv_layer_top());active=w;EditorWindow::SetTitle(w->root,"Events and actions");EditorWindow::SetCloseHandler(w->root,[]{Close();});
    lv_obj_set_size(w->root,std::min(780,lv_display_get_horizontal_resolution(nullptr)-40),std::min(920,lv_display_get_vertical_resolution(nullptr)-60));lv_obj_center(w->root);
    auto* body=EditorWindow::Content(w->root);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(body,16,0);lv_obj_set_style_pad_row(body,8,0);
    w->sourceLabel=Label(body,"");Label(body,"Actions run in list order. Failed validation stops later actions. Select a widget in the object tree to edit its events.");
    w->list=Select(w,body,"Saved actions","(No actions)");lv_obj_add_event_cb(w->list,[](lv_event_t* e){LoadAction(static_cast<Window*>(lv_event_get_user_data(e)));},LV_EVENT_VALUE_CHANGED,w);
    Button(body,"Add action",[w]{try{if(!w->source)throw std::invalid_argument("Select a widget first");w->actions.push_back({{"event","clicked"},{"action","toggle"},{"targetOid",-1}});Store(w,uint32_t(w->actions.size()-1));}catch(const std::exception& e){lv_label_set_text(w->status,e.what());}});
    w->event=Select(w,body,"When","Clicked\nValue changed\nReady / Enter");w->action=Select(w,body,"Action","Navigate to screen\nSet variable\nIncrement variable\nShow widget\nHide widget\nToggle widget\nValidate text box");
    w->target=Select(w,body,"Target widget (show / hide / toggle / validate)","This widget");w->screen=Input(w,body,"Screen number (navigate, starts at 1)","1");w->variable=Select(w,body,"Variable (set / increment)","(No variables)");w->value=Input(w,body,"Value (JSON value, or plain text)","1");
    w->required=lv_checkbox_create(body);lv_checkbox_set_text(w->required,"Required value (validate)");lv_obj_add_state(w->required,LV_STATE_CHECKED);w->minimum=Input(w,body,"Minimum numeric value (optional)");w->maximum=Input(w,body,"Maximum numeric value (optional)");
    w->conditionVariable=Select(w,body,"Only when","Always");w->conditionOp=Select(w,body,"Condition comparison","Equals\nDoes not equal\nGreater than\nLess than\nIs true / nonempty");w->conditionValue=Input(w,body,"Condition value","true");
    Button(body,"Save action",[w]{Save(w);});Button(body,"Remove selected action",[w]{try{auto i=lv_dropdown_get_selected(w->list);if(i<w->actions.size()){w->actions.erase(i);Store(w,0);}}catch(const std::exception& e){lv_label_set_text(w->status,e.what());}});
    w->status=Label(body,"");Refresh(w);
    w->timer=lv_timer_create([](lv_timer_t* t){auto* w=static_cast<Window*>(lv_timer_get_user_data(t));auto objects=w->properties->GetObjectTree()->GetObjects();if(w->source!=w->properties->GetSelectedObject()||objects!=w->objects)Refresh(w);},200,w);
    lv_obj_add_event_cb(w->root,[](lv_event_t* e){auto* w=static_cast<Window*>(lv_event_get_user_data(e));if(w->timer)lv_timer_delete(w->timer);active=nullptr;delete w;},LV_EVENT_DELETE,w);
}
}
