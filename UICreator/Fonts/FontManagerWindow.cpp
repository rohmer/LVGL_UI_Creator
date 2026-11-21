#include "FontManagerWindow.h"
#include "FontConverterWindow.h"
#include "../Windows/PropertyWindow.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include <functional>

namespace FontManagerWindow {
namespace {
struct Manager {PropertyWindow* properties;lv_obj_t *root,*list,*name,*details,*status;lv_timer_t* timer=nullptr;json displayed;};
lv_obj_t* active=nullptr;
void Button(lv_obj_t* parent,const char* text,std::function<void()> action) {
    auto* button=lv_button_create(parent);lv_obj_set_size(button,LV_PCT(100),38);auto* label=lv_label_create(button);lv_label_set_text(label,text);lv_obj_center(label);
    lv_obj_add_event_cb(button,[](lv_event_t* e){auto* fn=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete fn;else if(lv_event_get_code(e)==LV_EVENT_CLICKED){auto invoke=*fn;invoke();}},LV_EVENT_ALL,new std::function<void()>(std::move(action)));
}
std::string Selected(Manager* m) {const auto& fonts=m->properties->Fonts().ToJSON();const auto i=lv_dropdown_get_selected(m->list);return i<fonts.size()?fonts[i].at("id").get<std::string>():std::string{};}
void Detail(Manager* m) {
    const auto& fonts=m->properties->Fonts().ToJSON();const auto i=lv_dropdown_get_selected(m->list);
    if(i>=fonts.size()){lv_textarea_set_text(m->name,"");lv_label_set_text(m->details,"No project fonts. Convert a font to add it to this project.");return;}
    const auto& font=fonts[i];lv_textarea_set_text(m->name,font.at("name").get<std::string>().c_str());
    auto info="ID: "+font.at("id").get<std::string>()+"\nLine height: "+std::to_string(font["font"].value("lineHeight",0))+" px\nBpp: "+std::to_string(font["font"]["dsc"].value("bpp",0))+"\nWidgets using this font: "+std::to_string(m->properties->Fonts().Usage(m->properties->ProjectScreen(),Selected(m)));
    lv_label_set_text(m->details,info.c_str());
}
void Refresh(Manager* m) {
    auto id=Selected(m);const auto& fonts=m->properties->Fonts().ToJSON();std::string options;uint32_t index=0,selected=0;
    for(const auto& f:fonts){if(!options.empty())options+='\n';options+=f.at("name").get<std::string>();if(f.at("id")==id)selected=index;++index;}
    lv_dropdown_set_options(m->list,options.empty()?"(No project fonts)":options.c_str());lv_dropdown_set_selected(m->list,selected);m->displayed=fonts;Detail(m);
}
}
void Close() {if(active)lv_obj_delete(active);}
void Open(PropertyWindow* properties) {
    if(active){lv_obj_move_foreground(active);return;}
    auto* m=new Manager{};m->properties=properties;m->root=EditorWindow::Create(lv_layer_top());EditorWindow::SetCloseHandler(m->root,[m]{lv_obj_delete(m->root);});active=m->root;lv_obj_set_size(m->root,540,620);lv_obj_center(m->root);EditorWindow::SetTitle(m->root,"Project fonts");
    auto* body=EditorWindow::Content(m->root);lv_obj_set_style_pad_all(body,16,0);lv_obj_set_style_pad_row(body,12,0);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);
    auto label=[&](const char* text){auto* l=lv_label_create(body);lv_obj_set_width(l,LV_PCT(100));lv_label_set_text(l,text);return l;};
    label("Reuse converted fonts throughout this project. Font IDs stay stable when renamed. In-use fonts cannot be removed.");
    m->list=lv_dropdown_create(body);lv_obj_set_width(m->list,LV_PCT(100));lv_obj_add_event_cb(m->list,[](lv_event_t* e){Detail(static_cast<Manager*>(lv_event_get_user_data(e)));},LV_EVENT_VALUE_CHANGED,m);
    BindEditorControl(m->properties->GetKBGroup(),m->list);
    label("Font name");m->name=lv_textarea_create(body);lv_textarea_set_one_line(m->name,true);lv_obj_set_width(m->name,LV_PCT(100));
    BindEditorControl(m->properties->GetKBGroup(),m->name);
    m->details=label("");
    Button(body,"Convert / add font...",[m]{FontConverterWindow::Open(m->properties);});
    Button(body,"Rename",[m]{try{m->properties->Fonts().Rename(Selected(m),lv_textarea_get_text(m->name));Refresh(m);m->properties->SetSelectedObject(m->properties->GetSelectedObject(),false);lv_label_set_text(m->status,"Font renamed.");}catch(const std::exception& e){lv_label_set_text(m->status,e.what());}});
    Button(body,"Apply to selected widget",[m]{try{m->properties->Fonts().Apply(m->properties->GetSelectedObject(),Selected(m));m->properties->SetSelectedObject(m->properties->GetSelectedObject(),false);Detail(m);lv_label_set_text(m->status,"Font applied.");}catch(const std::exception& e){lv_label_set_text(m->status,e.what());}});
    Button(body,"Remove unused font",[m]{try{auto id=Selected(m);if(id.empty())throw std::invalid_argument("Select a font first");if(m->properties->Fonts().Usage(m->properties->ProjectScreen(),id))throw std::invalid_argument("This font is in use. Select Inherit / theme on its widgets before removing it.");m->properties->Fonts().Remove(id);Refresh(m);m->properties->SetSelectedObject(m->properties->GetSelectedObject(),false);}catch(const std::exception& e){lv_label_set_text(m->status,e.what());}});
    Button(body,"Close",[m]{lv_obj_delete(m->root);});m->status=label("");Refresh(m);
    m->timer=lv_timer_create([](lv_timer_t* t){auto* m=static_cast<Manager*>(lv_timer_get_user_data(t));if(m->displayed!=m->properties->Fonts().ToJSON())Refresh(m);},250,m);
    lv_obj_add_event_cb(m->root,[](lv_event_t* e){auto* m=static_cast<Manager*>(lv_event_get_user_data(e));if(m->timer)lv_timer_delete(m->timer);active=nullptr;delete m;},LV_EVENT_DELETE,m);
}
}
