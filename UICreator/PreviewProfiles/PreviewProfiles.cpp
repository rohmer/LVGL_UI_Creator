#include "PreviewProfiles.h"
#include "PreviewSurface.h"
#include "../ProjectThemes/AuthoringControls.h"
#include <algorithm>
#include <cctype>
#include <array>

namespace PreviewProfiles {
using json=nlohmann::json;
namespace {
using namespace ProjectAuthoring;
struct Window {
    DesignContext context;
    lv_obj_t *root=nullptr,*status=nullptr,*name=nullptr,*width=nullptr,*height=nullptr;
    std::array<lv_obj_t*,2> choices{},rotate{},hosts{},captions{};
    std::array<std::unique_ptr<PreviewSurface>,2> surfaces;
    json profiles;
    lv_timer_t* timer=nullptr;
    ~Window(){if(timer)lv_timer_delete(timer);surfaces={};}
};
Window* active=nullptr;
std::string ProfileLabels(const json& profiles){std::string text;for(const auto& item:profiles){if(!text.empty())text+='\n';text+=item.value("name",std::string())+"  "+std::to_string(item.value("width",0))+" x "+std::to_string(item.value("height",0));}return text;}
void Load(Window* w)
{
    try {
        const auto document=w->context.document();lv_obj_update_layout(w->root);
        for(size_t i=0;i<2;++i){const auto index=lv_dropdown_get_selected(w->choices[i]);if(index>=w->profiles.size())continue;
            const auto& profile=w->profiles[index];int width=profile.at("width"),height=profile.at("height");if(lv_obj_is_checked(w->rotate[i]))std::swap(width,height);
            if(!w->surfaces[i])w->surfaces[i]=std::make_unique<PreviewSurface>(w->hosts[i]);
            w->surfaces[i]->Load(document,width,height,w->context.sourceDirectory,true);
            lv_label_set_text_fmt(w->captions[i],"%d x %d  |  independent runtime",width,height);
        }
        lv_label_set_text(w->status,"Click a preview to test its controls or type. Each copy is independent. Resolution and orientation do not modify your design.");
    }catch(const std::exception& e){lv_label_set_text(w->status,e.what());}
}
void ProfileChoices(Window* w)
{
    w->profiles=Profiles(w->context.document());const auto labels=ProfileLabels(w->profiles);
    for(auto* choice:w->choices)lv_dropdown_set_options(choice,labels.c_str());
    lv_dropdown_set_selected(w->choices[0],0);lv_dropdown_set_selected(w->choices[1],2);
}
int Number(lv_obj_t* field){const std::string text=lv_textarea_get_text(field);size_t consumed=0;const int value=std::stoi(text,&consumed);if(consumed!=text.size())throw std::invalid_argument("Use whole-number dimensions");return value;}
}
void Close(){if(active){auto* root=active->root;EditorWindow::ReleaseFocus(root);lv_obj_delete(root);}}
lv_obj_t* ActiveWindow(){return active?active->root:nullptr;}
bool HandleEvent(const SDL_Event& event){
    if(!active)return false;
    if(lv_obj_is_hidden(active->root)||lv_obj_get_child(lv_layer_top(),-1)!=active->root){PreviewSurface::Blur();return false;}
    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN){
        lv_point_t point{int(event.button.x),int(event.button.y)};bool inside=false;
        for(const auto& surface:active->surfaces)if(surface&&surface->Image()) {
            auto native=point;lv_obj_transform_point(surface->Image(),&native,LV_OBJ_POINT_TRANSFORM_FLAG_INVERSE_RECURSIVE);lv_area_t area{};lv_obj_get_coords(surface->Image(),&area);
            if(native.x>=area.x1&&native.x<=area.x2&&native.y>=area.y1&&native.y<=area.y2)inside=true;
        }
        if(!inside)PreviewSurface::Blur();
        return false;
    }
    auto* surface=PreviewSurface::Focused();return surface&&surface->HandleEvent(event);
}
void Open(ProjectAuthoring::DesignContext context)
{
    Close();auto* w=new Window;w->context=std::move(context);active=w;
    w->root=EditorWindow::Create(lv_layer_top());lv_obj_set_size(w->root,LV_PCT(95),LV_PCT(94));lv_obj_center(w->root);EditorWindow::SetTitle(w->root,"Device preview comparison");
    lv_obj_add_event_cb(w->root,[](lv_event_t* e){auto* w=static_cast<Window*>(lv_event_get_user_data(e));if(active==w)active=nullptr;delete w;},LV_EVENT_DELETE,w);
    EditorWindow::SetCloseHandler(w->root,[]{Close();});auto* body=EditorWindow::Content(w->root);Column(body);lv_obj_set_scroll_dir(body,LV_DIR_VER);
    EditorStyle::Help(body,"Preview the current screen at two device sizes. Flex and Grid reflow natively; fixed pixel coordinates retain their authored meaning.");
    auto* columns=Row(body);lv_obj_set_flex_flow(columns,LV_FLEX_FLOW_ROW);lv_obj_set_flex_grow(columns,1);lv_obj_set_height(columns,0);lv_obj_set_style_min_height(columns,240,0);
    for(size_t i=0;i<2;++i){auto* column=lv_obj_create(columns);Column(column);lv_obj_set_width(column,0);lv_obj_set_height(column,LV_PCT(100));lv_obj_set_flex_grow(column,1);lv_obj_set_scrollable(column,false);lv_obj_set_style_pad_all(column,0,0);
        w->choices[i]=lv_dropdown_create(column);lv_obj_set_width(w->choices[i],LV_PCT(100));EditorStyle::Control(w->choices[i]);Focus(w->choices[i],w->context.keyboardGroup);
        w->rotate[i]=lv_checkbox_create(column);lv_checkbox_set_text(w->rotate[i],"Swap orientation");Focus(w->rotate[i],w->context.keyboardGroup);
        w->captions[i]=EditorStyle::Help(column,"");w->hosts[i]=lv_obj_create(column);lv_obj_set_size(w->hosts[i],LV_PCT(100),0);lv_obj_set_flex_grow(w->hosts[i],1);lv_obj_set_scrollable(w->hosts[i],false);lv_obj_set_style_pad_all(w->hosts[i],0,0);lv_obj_set_style_bg_color(w->hosts[i],lv_color_hex(0x111820),0);
        for(auto* control:{w->choices[i],w->rotate[i]})lv_obj_add_event_cb(control,[](lv_event_t* e){Load(static_cast<Window*>(lv_event_get_user_data(e)));},LV_EVENT_VALUE_CHANGED,w);
    }
    auto* actions=Row(body);Button(actions,"Refresh previews",[w]{Load(w);},170,w->context.keyboardGroup);
    Button(actions,"Edit Flex / Grid...",[w]{OpenLayout(w->context);},190,w->context.keyboardGroup);
    Button(actions,"Remember comparison",[w]{try{auto doc=w->context.document();auto& metadata=doc["previewProfiles"];if(!metadata.is_object())metadata=json::object();metadata["version"]=1;metadata["left"]=w->profiles.at(lv_dropdown_get_selected(w->choices[0])).at("id");metadata["right"]=w->profiles.at(lv_dropdown_get_selected(w->choices[1])).at("id");metadata["leftRotated"]=lv_obj_is_checked(w->rotate[0]);metadata["rightRotated"]=lv_obj_is_checked(w->rotate[1]);if(w->context.applyMetadata)w->context.applyMetadata({{"previewProfiles",doc["previewProfiles"]}});else w->context.apply(std::move(doc));if(active==w)lv_label_set_text(w->status,"Comparison saved in this project. Undo is available.");}catch(const std::exception& e){if(active==w)lv_label_set_text(w->status,e.what());}},210,w->context.keyboardGroup);
    auto* custom=Row(body);w->name=Field(custom,"Custom profile name",w->context.keyboardGroup);lv_obj_set_width(w->name,220);
    w->width=Field(custom,"Width",w->context.keyboardGroup);lv_obj_set_width(w->width,90);lv_textarea_set_text(w->width,"640");
    w->height=Field(custom,"Height",w->context.keyboardGroup);lv_obj_set_width(w->height,90);lv_textarea_set_text(w->height,"480");
    Button(custom,"Save profile",[w]{try{auto doc=w->context.document();SaveProfile(doc,lv_textarea_get_text(w->name),Number(w->width),Number(w->height));if(w->context.applyMetadata)w->context.applyMetadata({{"previewProfiles",doc["previewProfiles"]}});else w->context.apply(std::move(doc));if(active==w){ProfileChoices(w);Load(w);lv_label_set_text(w->status,"Named profile saved. It is available in both preview lists.");}}catch(const std::exception& e){if(active==w)lv_label_set_text(w->status,e.what());}},150,w->context.keyboardGroup);
    w->status=EditorStyle::Help(body,"");ProfileChoices(w);
    auto metadata=w->context.document().value("previewProfiles",json::object());if(!metadata.is_object())metadata=json::object();for(size_t i=0;i<2;++i){const auto key=i?"right":"left";const auto id=metadata.contains(key)&&metadata[key].is_string()?metadata[key].get<std::string>():std::string();for(size_t j=0;j<w->profiles.size();++j)if(w->profiles[j]["id"]==id)lv_dropdown_set_selected(w->choices[i],static_cast<uint32_t>(j));const auto rotated=i?"rightRotated":"leftRotated";lv_obj_set_state(w->rotate[i],LV_STATE_CHECKED,metadata.contains(rotated)&&metadata[rotated].is_boolean()&&metadata[rotated].get<bool>());}
    Load(w);w->timer=lv_timer_create([](lv_timer_t* timer){auto* w=static_cast<Window*>(lv_timer_get_user_data(timer));for(size_t i=0;i<2;++i)if(w->surfaces[i])w->surfaces[i]->Fit(lv_obj_get_content_width(w->hosts[i]),lv_obj_get_content_height(w->hosts[i]));},150,w);
}
}
