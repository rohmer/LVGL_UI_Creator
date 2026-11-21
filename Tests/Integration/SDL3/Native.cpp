#define SDL_MAIN_HANDLED
#include "../../../UICreator/Utils/SdlSimulator.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("Line ")+std::to_string(__LINE__)+": " #x);}while(0)
void Drain(SdlSimulator& sim){for(int i=0;i<8;++i)CHECK(sim.ProcessEvents());}
void Key(SdlSimulator& sim,SDL_WindowID id,SDL_Keycode key,SDL_Keymod modifiers=SDL_KMOD_NONE,bool repeat=false){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.windowID=id;event.key.key=key;event.key.mod=modifiers;event.key.down=true;event.key.repeat=repeat;CHECK(SDL_PushEvent(&event));Drain(sim);}
int main(){try {
    CHECK(SDL_VERSIONNUM_MAJOR(SDL_GetVersion())==3);
    SdlSimulator sim;CHECK(sim.Initialize(320,240));auto* display=lv_display_get_default();auto* window=lv_sdl_window_get_window(display);CHECK(window);
    auto id=SDL_GetWindowID(window);Drain(sim);int width=0,height=0;CHECK(SDL_GetWindowSize(window,&width,&height));CHECK(width==320 && height==240);
    auto* field=lv_textarea_create(lv_screen_active());lv_obj_set_size(field,180,40);lv_textarea_set_text(field,"");
    auto* group=lv_group_create();lv_group_add_obj(group,field);lv_indev_set_group(sim.GetKeyboard(),group);lv_group_focus_obj(field);lv_group_set_editing(group,true);
    int shortcuts=0;bool shifted=false;
    sim.SetShortcutHandler([&](int key,bool ctrl,bool shift,bool alt){if(key=='z' && ctrl && !alt){++shortcuts;shifted=shift;return true;}return false;});
    Key(sim,id,SDLK_Z,SDL_KMOD_CTRL);CHECK(shortcuts==1);CHECK(std::strlen(lv_textarea_get_text(field))==0);
    Key(sim,id,SDLK_Z,SDL_KMOD_CTRL,true);CHECK(shortcuts==1);
    Key(sim,id,SDLK_Z,(SDL_Keymod)(SDL_KMOD_CTRL|SDL_KMOD_SHIFT));CHECK(shortcuts==2 && shifted);
    SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.windowID=id;event.text.text="\xc3\xa9";CHECK(SDL_PushEvent(&event));Drain(sim);CHECK(std::strcmp(lv_textarea_get_text(field),"\xc3\xa9")==0);
    Key(sim,id,SDLK_BACKSPACE);CHECK(std::strlen(lv_textarea_get_text(field))==0);
    auto* button=lv_button_create(lv_screen_active());lv_obj_set_pos(button,20,70);lv_obj_set_size(button,100,45);int clicks=0;
    lv_obj_add_event_cb(button,[](lv_event_t* e){++*static_cast<int*>(lv_event_get_user_data(e));},LV_EVENT_CLICKED,&clicks);lv_obj_update_layout(lv_screen_active());
    event={};event.type=SDL_EVENT_MOUSE_MOTION;event.motion.windowID=id;event.motion.x=50;event.motion.y=90;CHECK(SDL_PushEvent(&event));
    event={};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.windowID=id;event.button.button=SDL_BUTTON_LEFT;event.button.down=true;event.button.x=50;event.button.y=90;CHECK(SDL_PushEvent(&event));Drain(sim);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;CHECK(SDL_PushEvent(&event));Drain(sim);CHECK(clicks==1);
    event={};event.type=SDL_EVENT_FINGER_DOWN;event.tfinger.windowID=id;event.tfinger.touchID=1;event.tfinger.fingerID=1;event.tfinger.x=0.2f;event.tfinger.y=0.4f;event.tfinger.pressure=1;CHECK(SDL_PushEvent(&event));Drain(sim);
    event.type=SDL_EVENT_FINGER_UP;event.tfinger.pressure=0;CHECK(SDL_PushEvent(&event));Drain(sim);CHECK(clicks==2);
    auto* scroll=lv_obj_create(lv_screen_active());lv_obj_set_pos(scroll,0,160);lv_obj_set_size(scroll,200,70);auto* child=lv_obj_create(scroll);lv_obj_set_pos(child,0,200);lv_obj_set_size(child,40,20);lv_obj_update_layout(lv_screen_active());
    event={};event.type=SDL_EVENT_MOUSE_WHEEL;event.wheel.windowID=id;event.wheel.mouse_x=100;event.wheel.mouse_y=190;event.wheel.y=-3;CHECK(SDL_PushEvent(&event));Drain(sim);CHECK(lv_obj_get_scroll_y(scroll)>0);
    event={};event.type=SDL_EVENT_WINDOW_RESIZED;event.window.windowID=id;event.window.data1=640;event.window.data2=480;CHECK(SDL_PushEvent(&event));Drain(sim);CHECK(lv_display_get_horizontal_resolution(display)==640);CHECK(lv_display_get_vertical_resolution(display)==480);
    lv_group_delete(group);event={};event.type=SDL_EVENT_QUIT;CHECK(SDL_PushEvent(&event));bool closed=false;for(int i=0;i<20;++i)if(!sim.ProcessEvents()){closed=true;break;}CHECK(closed);
    std::cout<<"Native SDL3 input, Unicode, shortcuts, touch, scrolling, resize and close passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

