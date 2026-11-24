#include "PreviewSurface.h"
#include "../Drivers/SDL3/NativeSDL3.h"
#include "../../CodeGeneration/ProjectAssets.h"
#include <Serialization/Protobuf.h>
#include <Serialization/ProjectDesign.h>
#include <src/misc/cache/instance/lv_image_cache.h>
#include <algorithm>
#include <deque>
#include <vector>
#include <stdexcept>

namespace PreviewProfiles {
namespace {PreviewSurface* focused=nullptr;}
struct PreviewSurface::Impl {
    PreviewSurface* owner;
    lv_obj_t *parent=nullptr,*image=nullptr,*active=nullptr;
    lv_display_t* display=nullptr;
    lvgl_runtime_t* runtime=nullptr;
    lv_group_t* group=nullptr;
    lv_indev_t *pointer=nullptr,*keyboard=nullptr;
    lv_timer_t* timer=nullptr;
    lv_image_dsc_t descriptor{};
    std::vector<uint32_t> pixels;
    std::vector<lv_obj_t*> focusable;
    std::deque<lv_indev_data_t> touches;
    std::deque<uint32_t> keys;
    lv_indev_data_t touch{};
    uint32_t key=0;
    bool keyDown=false;
    int width=0,height=0;
    explicit Impl(PreviewSurface* value):owner(value){}
    void SyncFocus(){
        if(!display||!group)return;
        auto* screen=lv_display_get_screen_active(display);if(active==screen)return;
        active=screen;lv_group_remove_all_objs(group);
        for(auto* object:focusable)if(object&&lv_obj_get_screen(object)==active)lv_group_add_obj(group,object);
    }
    void Pump(){
        SyncFocus();
        while(pointer&&!touches.empty()){touch=touches.front();touches.pop_front();lv_indev_read(pointer);SyncFocus();}
        while(keyboard&&!keys.empty()){key=keys.front();keys.pop_front();keyDown=true;lv_indev_read(keyboard);keyDown=false;lv_indev_read(keyboard);SyncFocus();}
    }
};
PreviewSurface::PreviewSurface(lv_obj_t* parent):state(std::make_unique<Impl>(this))
{
    auto& s=*state;s.parent=parent;s.image=lv_image_create(parent);
    lv_obj_set_scrollable(s.image,false);lv_obj_set_click_focusable(s.image,false);
    lv_obj_set_style_transform_pivot_x(s.image,0,0);lv_obj_set_style_transform_pivot_y(s.image,0,0);
    lv_obj_add_event_cb(s.image,[](lv_event_t* event){
        auto* self=static_cast<Impl*>(lv_event_get_user_data(event));const auto code=lv_event_get_code(event);
        if(code==LV_EVENT_DELETE){self->image=nullptr;return;}
        if(!self->pointer||!self->runtime)return;
        if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_PRESSING&&code!=LV_EVENT_RELEASED&&code!=LV_EVENT_PRESS_LOST)return;
        auto* input=lv_indev_active();if(!input)return;
        if(code==LV_EVENT_PRESSED)focused=self->owner;
        lv_point_t point{};lv_indev_get_point(input,&point);lv_obj_transform_point(self->image,&point,LV_OBJ_POINT_TRANSFORM_FLAG_INVERSE_RECURSIVE);
        lv_area_t area{};lv_obj_get_coords(self->image,&area);
        lv_indev_data_t data{};data.point={std::clamp(point.x-area.x1,0,self->width-1),std::clamp(point.y-area.y1,0,self->height-1)};
        data.state=(code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST)?LV_INDEV_STATE_RELEASED:LV_INDEV_STATE_PRESSED;
        if(self->touches.size()<128)self->touches.push_back(data);
    },LV_EVENT_ALL,&s);
}
PreviewSurface::~PreviewSurface(){Clear();if(state->image)lv_obj_delete(state->image);}
void PreviewSurface::Freeze()
{
    auto& s=*state;if(focused==this)focused=nullptr;
    if(s.timer){lv_timer_delete(s.timer);s.timer=nullptr;}
    if(s.pointer){lv_indev_delete(s.pointer);s.pointer=nullptr;}
    if(s.keyboard){lv_indev_delete(s.keyboard);s.keyboard=nullptr;}
    if(s.runtime){lvgl_runtime_destroy(s.runtime);s.runtime=nullptr;}
    if(s.group){lv_group_delete(s.group);s.group=nullptr;}
    if(s.display){lv_display_delete(s.display);s.display=nullptr;}
    s.focusable.clear();s.touches.clear();s.keys.clear();s.active=nullptr;
    if(s.image)lv_obj_set_clickable(s.image,false);
}
void PreviewSurface::Clear()
{
    Freeze();auto& s=*state;
    if(s.image)lv_image_set_src(s.image,nullptr);
    lv_image_cache_drop(&s.descriptor);s.pixels.clear();s.descriptor={};s.width=s.height=0;
}
void PreviewSurface::Load(nlohmann::json document,int width,int height,const std::string& sourceDirectory,bool interactive)
{
    Clear();auto& s=*state;
    document=AtResolution(std::move(document),width,height);
    Serialization::ProjectDesign::Materialize(document,true);
    CGeneration::EmbedLocalImages(document,std::filesystem::u8path(sourceDirectory));document["runtimeTarget"]="9.6";
    const auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",document);
    auto* editorDisplay=lv_display_get_default();auto* editorGroup=lv_group_get_default();
    try {
        s.width=width;s.height=height;s.pixels.resize(size_t(width)*height);
        s.display=lv_display_create(width,height);if(!s.display)throw std::runtime_error("Cannot create preview display");
        lv_display_set_default(editorDisplay);lv_display_set_color_format(s.display,LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_user_data(s.display,&s);
        lv_display_set_flush_cb(s.display,[](lv_display_t* display,const lv_area_t*,uint8_t*){
            auto* self=static_cast<Impl*>(lv_display_get_user_data(display));if(self->image)lv_obj_invalidate(self->image);lv_display_flush_ready(display);
        });
        lv_display_set_buffers(s.display,s.pixels.data(),nullptr,uint32_t(s.pixels.size()*4),LV_DISPLAY_RENDER_MODE_DIRECT);
        s.group=lv_group_create();lv_group_set_default(s.group);
        lvgl_runtime_options_t options{s.display,0,true};char error[1024]{};
        const bool loaded=lvgl_runtime_load_memory(bytes.data(),bytes.size(),&options,&s.runtime,error,sizeof(error));
        lv_group_set_default(editorGroup);if(!loaded)throw std::runtime_error(error);
        if(interactive){
            for(uint32_t i=0;i<lv_group_get_obj_count(s.group);++i){auto* object=lv_group_get_obj_by_index(s.group,i);s.focusable.push_back(object);lv_obj_add_event_cb(object,[](lv_event_t* e){auto* self=static_cast<Impl*>(lv_event_get_user_data(e));for(auto& item:self->focusable)if(item==lv_event_get_current_target_obj(e))item=nullptr;self->active=nullptr;},LV_EVENT_DELETE,&s);}
            s.pointer=lv_indev_create();lv_indev_set_type(s.pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_display(s.pointer,s.display);lv_indev_set_mode(s.pointer,LV_INDEV_MODE_EVENT);lv_indev_set_user_data(s.pointer,&s);
            lv_indev_set_read_cb(s.pointer,[](lv_indev_t* input,lv_indev_data_t* data){*data=static_cast<Impl*>(lv_indev_get_user_data(input))->touch;});
            s.keyboard=lv_indev_create();lv_indev_set_type(s.keyboard,LV_INDEV_TYPE_KEYPAD);lv_indev_set_display(s.keyboard,s.display);lv_indev_set_group(s.keyboard,s.group);lv_indev_set_mode(s.keyboard,LV_INDEV_MODE_EVENT);lv_indev_set_user_data(s.keyboard,&s);
            lv_indev_set_read_cb(s.keyboard,[](lv_indev_t* input,lv_indev_data_t* data){auto* self=static_cast<Impl*>(lv_indev_get_user_data(input));data->key=self->key;data->state=self->keyDown?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;});
            lv_obj_set_clickable(s.image,true);lv_obj_set_press_lock(s.image,true);
            s.SyncFocus();s.timer=lv_timer_create([](lv_timer_t* timer){static_cast<Impl*>(lv_timer_get_user_data(timer))->Pump();},5,&s);
        }else lv_obj_set_clickable(s.image,false);
        s.descriptor.header.magic=LV_IMAGE_HEADER_MAGIC;s.descriptor.header.cf=LV_COLOR_FORMAT_XRGB8888;s.descriptor.header.w=width;s.descriptor.header.h=height;s.descriptor.header.stride=width*4;
        s.descriptor.data_size=uint32_t(s.pixels.size()*4);s.descriptor.data=reinterpret_cast<const uint8_t*>(s.pixels.data());
        lv_image_set_src(s.image,&s.descriptor);Fit(lv_obj_get_content_width(s.parent),lv_obj_get_content_height(s.parent));
        lv_refr_now(s.display);
    }catch(...){lv_group_set_default(editorGroup);lv_display_set_default(editorDisplay);Clear();throw;}
}
void PreviewSurface::Fit(int width,int height)
{
    auto& s=*state;if(!s.image||!s.width||!s.height)return;
    const auto scale=std::clamp((std::min)(double(width)/s.width,double(height)/s.height),0.05,2.0);
    lv_obj_set_style_transform_scale_x(s.image,int(scale*256),0);lv_obj_set_style_transform_scale_y(s.image,int(scale*256),0);
    lv_obj_set_pos(s.image,(std::max)(0,int((width-s.width*scale)/2)),(std::max)(0,int((height-s.height*scale)/2)));
}
lv_obj_t* PreviewSurface::Image()const{return state->image;}
lvgl_runtime_t* PreviewSurface::Runtime()const{return state->runtime;}
int PreviewSurface::Width()const{return state->width;}
int PreviewSurface::Height()const{return state->height;}
uint32_t PreviewSurface::Pixel(int x,int y)const{return x>=0&&y>=0&&x<state->width&&y<state->height?state->pixels[size_t(y)*state->width+x]:0;}
uint64_t PreviewSurface::PixelHash()const{uint64_t hash=1469598103934665603ull;for(auto pixel:state->pixels){hash^=pixel;hash*=1099511628211ull;}return hash;}
PreviewSurface* PreviewSurface::Focused(){return focused;}
void PreviewSurface::Blur(){focused=nullptr;}
bool PreviewSurface::HandleEvent(const SDL_Event& event)
{
    auto& s=*state;if(!s.keyboard||focused!=this)return false;
    if(event.type==SDL_EVENT_TEXT_INPUT){
#ifdef __APPLE__
        if(SDL_GetModState()&SDL_KMOD_GUI)return true;
#endif
        const auto* p=reinterpret_cast<const unsigned char*>(event.text.text);while(p&&*p&&s.keys.size()<1024){uint32_t cp=*p++;int n=0;if(cp>=0xf0){cp&=7;n=3;}else if(cp>=0xe0){cp&=15;n=2;}else if(cp>=0xc0){cp&=31;n=1;}while(n--&&*p)cp=(cp<<6)|(*p++&63);if(cp>=32&&cp<=0x10ffff)s.keys.push_back(cp);}return true;}
    if(event.type==SDL_EVENT_TEXT_EDITING||event.type==SDL_EVENT_KEY_UP)return true;
    if(event.type!=SDL_EVENT_KEY_DOWN)return false;
    if(uicreator_sdl3_primary_modifier(event.key.mod)||(event.key.mod&SDL_KMOD_ALT))return false;
    uint32_t key=0;switch(event.key.key){case SDLK_BACKSPACE:key=LV_KEY_BACKSPACE;break;case SDLK_DELETE:key=LV_KEY_DEL;break;case SDLK_RETURN:case SDLK_KP_ENTER:key=LV_KEY_ENTER;break;case SDLK_LEFT:key=LV_KEY_LEFT;break;case SDLK_RIGHT:key=LV_KEY_RIGHT;break;case SDLK_UP:key=LV_KEY_UP;break;case SDLK_DOWN:key=LV_KEY_DOWN;break;case SDLK_HOME:key=LV_KEY_HOME;break;case SDLK_END:key=LV_KEY_END;break;case SDLK_TAB:key=event.key.mod&SDL_KMOD_SHIFT?LV_KEY_PREV:LV_KEY_NEXT;break;default:break;}
    if(key&&s.keys.size()<1024)s.keys.push_back(key);return true;
}
}
