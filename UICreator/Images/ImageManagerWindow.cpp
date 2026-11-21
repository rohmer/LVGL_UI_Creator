#include "ImageManagerWindow.h"
#include "ImageConverterWindow.h"
#include "../Windows/PropertyWindow.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include <functional>

namespace ImageManagerWindow {
namespace {
struct Manager {PropertyWindow* properties;lv_obj_t *root,*list,*name,*details,*status,*preview;lv_timer_t* timer=nullptr;json displayed;};
lv_obj_t* active=nullptr;
void Button(lv_obj_t* parent,const char* text,std::function<void()> action) {
    auto* button=lv_button_create(parent);lv_obj_set_size(button,LV_PCT(100),38);auto* label=lv_label_create(button);lv_label_set_text(label,text);lv_obj_center(label);
    lv_obj_add_event_cb(button,[](lv_event_t* e){auto* fn=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete fn;else if(lv_event_get_code(e)==LV_EVENT_CLICKED){auto invoke=*fn;invoke();}},LV_EVENT_ALL,new std::function<void()>(std::move(action)));
}
std::string Selected(Manager* m) {const auto& images=m->properties->Images().ToJSON();const auto i=lv_dropdown_get_selected(m->list);return i<images.size()?images[i].at("id").get<std::string>():std::string{};}
void Detail(Manager* m) {
    const auto& images=m->properties->Images().ToJSON();const auto i=lv_dropdown_get_selected(m->list);
    if(i>=images.size()){lv_textarea_set_text(m->name,"");lv_label_set_text(m->details,"No project images. Import an image to add it to this project.");return;}
    const auto& image=images[i];lv_textarea_set_text(m->name,image.at("name").get<std::string>().c_str());
    auto info="ID: "+image.at("id").get<std::string>()+"\nWidgets using this image: "+std::to_string(m->properties->Images().Usage(m->properties->ProjectScreen(),Selected(m)));
    Serialization::ImageAsset::Restore(m->preview,image.at("image"));lv_obj_set_size(m->preview,180,100);lv_image_set_inner_align(m->preview,LV_IMAGE_ALIGN_CONTAIN);
    lv_label_set_text(m->details,info.c_str());
}
void Refresh(Manager* m) {
    auto id=Selected(m);const auto& images=m->properties->Images().ToJSON();std::string options;uint32_t index=0,selected=0;
    for(const auto& f:images){if(!options.empty())options+='\n';options+=f.at("name").get<std::string>();if(f.at("id")==id)selected=index;++index;}
    lv_dropdown_set_options(m->list,options.empty()?"(No project images)":options.c_str());lv_dropdown_set_selected(m->list,selected);m->displayed=images;Detail(m);
}
}
void Close() {if(active)lv_obj_delete(active);}
void Open(PropertyWindow* properties) {
    if(active){lv_obj_move_foreground(active);return;}
    auto* m=new Manager{};m->properties=properties;m->root=EditorWindow::Create(lv_layer_top());EditorWindow::SetCloseHandler(m->root,[m]{lv_obj_delete(m->root);});active=m->root;lv_obj_set_size(m->root,std::min(560,lv_display_get_horizontal_resolution(nullptr)-40),std::min(760,lv_display_get_vertical_resolution(nullptr)-60));lv_obj_center(m->root);EditorWindow::SetTitle(m->root,"Project images");
    auto* body=EditorWindow::Content(m->root);lv_obj_set_style_pad_all(body,16,0);lv_obj_set_style_pad_row(body,12,0);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);
    auto label=[&](const char* text){auto* l=lv_label_create(body);lv_obj_set_width(l,LV_PCT(100));lv_label_set_text(l,text);return l;};
    label("Reuse converted images throughout this project. Image IDs stay stable when renamed. In-use images cannot be removed.");
    m->list=lv_dropdown_create(body);lv_obj_set_width(m->list,LV_PCT(100));lv_obj_add_event_cb(m->list,[](lv_event_t* e){Detail(static_cast<Manager*>(lv_event_get_user_data(e)));},LV_EVENT_VALUE_CHANGED,m);
    BindEditorControl(m->properties->GetKBGroup(),m->list);
    label("Image name");m->name=lv_textarea_create(body);lv_textarea_set_one_line(m->name,true);lv_obj_set_width(m->name,LV_PCT(100));
    BindEditorControl(m->properties->GetKBGroup(),m->name);
    m->details=label("");m->preview=lv_image_create(body);Serialization::LVObject::FromJSON(json::object(),m->preview);lv_obj_set_size(m->preview,180,100);
    Button(body,"Import / convert images...",[m]{ImageConverterWindow::Open(m->properties,json::object());});
    Button(body,"Convert selected image...",[m]{try{ImageConverterWindow::Open(m->properties,m->properties->Images().Get(Selected(m)));}catch(const std::exception& e){lv_label_set_text(m->status,e.what());}});
    Button(body,"Rename",[m]{try{m->properties->Images().Rename(Selected(m),lv_textarea_get_text(m->name));Refresh(m);m->properties->SetSelectedObject(m->properties->GetSelectedObject(),false);lv_label_set_text(m->status,"Image renamed.");}catch(const std::exception& e){lv_label_set_text(m->status,e.what());}});
    Button(body,"Apply to selected widget",[m]{try{m->properties->Images().Apply(m->properties->GetSelectedObject(),Selected(m));m->properties->SetSelectedObject(m->properties->GetSelectedObject(),false);Detail(m);lv_label_set_text(m->status,"Image applied.");}catch(const std::exception& e){lv_label_set_text(m->status,e.what());}});
    Button(body,"Remove unused image",[m]{try{auto id=Selected(m);if(id.empty())throw std::invalid_argument("Select an image first");if(m->properties->Images().Usage(m->properties->ProjectScreen(),id))throw std::invalid_argument("This image is in use. Choose another image on its widgets before removing it.");m->properties->Images().Remove(id);Refresh(m);m->properties->SetSelectedObject(m->properties->GetSelectedObject(),false);}catch(const std::exception& e){lv_label_set_text(m->status,e.what());}});
    Button(body,"Close",[m]{lv_obj_delete(m->root);});m->status=label("");Refresh(m);
    m->timer=lv_timer_create([](lv_timer_t* t){auto* m=static_cast<Manager*>(lv_timer_get_user_data(t));if(m->displayed!=m->properties->Images().ToJSON())Refresh(m);},250,m);
    lv_obj_add_event_cb(m->root,[](lv_event_t* e){auto* m=static_cast<Manager*>(lv_event_get_user_data(e));if(m->timer)lv_timer_delete(m->timer);active=nullptr;delete m;},LV_EVENT_DELETE,m);
}
}
