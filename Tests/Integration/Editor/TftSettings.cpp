#include "TftSettings.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include <src/widgets/buttonmatrix/lv_buttonmatrix_private.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char* message) { if(!condition) throw std::runtime_error(message); }
void Drain(SdlSimulator& simulator) { for(int frame=0;frame<12;++frame) Require(simulator.ProcessEvents(),"SDL stopped during TFT input test"); }
lv_obj_t* Label(lv_obj_t* parent, const std::function<bool(const std::string&)>& matches) {
    if(lv_obj_has_class(parent,&lv_label_class)&&matches(lv_label_get_text(parent)))return parent;
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i)
        if(auto* found=Label(lv_obj_get_child(parent,i),matches))return found;
    return nullptr;
}
lv_obj_t* Label(lv_obj_t* parent, const char* text) { return Label(parent,[text](const std::string& value){return value==text;}); }
lv_obj_t* ParentButton(lv_obj_t* label) {
    for(auto* object=label;object;object=lv_obj_get_parent(object))if(lv_obj_has_class(object,&lv_button_class))return object;
    return nullptr;
}
lv_obj_t* FindClass(lv_obj_t* parent, const lv_obj_class_t* type) {
    if(lv_obj_has_class(parent,type))return parent;
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i)
        if(auto* found=FindClass(lv_obj_get_child(parent,i),type))return found;
    return nullptr;
}
void ClickAt(float x,float y,SdlSimulator& simulator) {
    const auto id=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=id;motion.motion.x=x;motion.motion.y=y;
    Require(SDL_PushEvent(&motion),"Cannot queue TFT pointer motion");
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.windowID=id;
    event.button.button=SDL_BUTTON_LEFT;event.button.down=true;event.button.x=x;event.button.y=y;
    Require(SDL_PushEvent(&event),"Cannot queue TFT pointer press");Drain(simulator);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;
    Require(SDL_PushEvent(&event),"Cannot queue TFT pointer release");Drain(simulator);
}
void Click(lv_obj_t* object,SdlSimulator& simulator) {
    Require(object&&!lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN),"TFT pointer target is missing or hidden");
    lv_obj_update_layout(lv_screen_active());lv_area_t area{};lv_obj_get_coords(object,&area);
    lv_obj_get_transformed_area(object,&area,LV_OBJ_POINT_TRANSFORM_FLAG_RECURSIVE);
    ClickAt(float((area.x1+area.x2)/2),float((area.y1+area.y2)/2),simulator);
}
void Text(const char* text,SdlSimulator& simulator) {
    SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text=text;
    event.text.windowID=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    Require(SDL_PushEvent(&event),"Cannot queue TFT physical text input");Drain(simulator);
}
void Key(SDL_Keycode key,SDL_Scancode scan,SdlSimulator& simulator,const char* printable=nullptr) {
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=key;event.key.scancode=scan;
    event.key.windowID=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));event.key.down=true;
    Require(SDL_PushEvent(&event),"Cannot queue TFT physical key");
    if(printable)Text(printable,simulator);
    event.type=SDL_EVENT_KEY_UP;event.key.down=false;
    Require(SDL_PushEvent(&event),"Cannot queue TFT physical key release");Drain(simulator);
}
void Digits(const char* value,SdlSimulator& simulator,bool keypad=false) {
    const SDL_Keycode keys[]={SDLK_KP_0,SDLK_KP_1,SDLK_KP_2,SDLK_KP_3,SDLK_KP_4,SDLK_KP_5,SDLK_KP_6,SDLK_KP_7,SDLK_KP_8,SDLK_KP_9};
    const SDL_Scancode scans[]={SDL_SCANCODE_KP_0,SDL_SCANCODE_KP_1,SDL_SCANCODE_KP_2,SDL_SCANCODE_KP_3,SDL_SCANCODE_KP_4,SDL_SCANCODE_KP_5,SDL_SCANCODE_KP_6,SDL_SCANCODE_KP_7,SDL_SCANCODE_KP_8,SDL_SCANCODE_KP_9};
    for(const char* digit=value;*digit;++digit){const int index=*digit-'0';const char text[]={*digit,0};
        Key(keypad?keys[index]:SDL_Keycode(*digit),keypad?scans[index]:index==0?SDL_SCANCODE_0:SDL_Scancode(SDL_SCANCODE_1+index-1),simulator,text);
    }
}
void Clear(lv_obj_t* field,SdlSimulator& simulator) {
    const auto length=std::string(lv_textarea_get_text(field)).size();
    Key(SDLK_HOME,SDL_SCANCODE_HOME,simulator);
    for(size_t index=0;index<length;++index)Key(SDLK_DELETE,SDL_SCANCODE_DELETE,simulator);
    Require(std::string(lv_textarea_get_text(field)).empty(),"Physical Home/Delete did not clear the TFT field");
}
bool Ready(lv_obj_t* button) { return !lv_obj_has_flag(button,LV_OBJ_FLAG_HIDDEN)&&!lv_obj_has_state(button,LV_STATE_DISABLED); }
lv_obj_t* OpenSettings(SdlSimulator& simulator) {
    auto* title=Label(lv_screen_active(),[](const std::string& text){return text.find("Simulator [Size:")==0;});
    Require(title,"Simulator title is missing");
    Click(ParentButton(Label(lv_obj_get_parent(title),LV_SYMBOL_SETTINGS)),simulator);
    auto* settings=Label(lv_screen_active(),"TFT Settings");Require(settings,"TFT Settings did not open");
    auto* window=lv_obj_get_parent(lv_obj_get_parent(settings));
    Require(!lv_obj_has_flag(window,LV_OBJ_FLAG_HIDDEN),"TFT Settings stayed hidden");return window;
}
std::vector<lv_obj_t*> Fields(lv_obj_t* window) {
    std::vector<lv_obj_t*> fields;auto* content=EditorWindow::Content(window);
    for(uint32_t i=0;i<lv_obj_get_child_count(content);++i){auto* child=lv_obj_get_child(content,i);if(lv_obj_has_class(child,&lv_textarea_class))fields.push_back(child);}
    Require(fields.size()==2,"Expected TFT width and height text fields");
    std::sort(fields.begin(),fields.end(),[](lv_obj_t* a,lv_obj_t* b){return lv_obj_get_y(a)<lv_obj_get_y(b);});return fields;
}
void Keypad(lv_obj_t* keyboard,const char* text,SdlSimulator& simulator) {
    Require(keyboard&&!lv_obj_has_flag(keyboard,LV_OBJ_FLAG_HIDDEN),"TFT on-screen keypad is not visible");lv_obj_update_layout(keyboard);
    auto* matrix=reinterpret_cast<lv_buttonmatrix_t*>(keyboard);
    for(uint32_t index=0;index<matrix->btn_cnt;++index)if(std::string(lv_buttonmatrix_get_button_text(keyboard,index))==text){
        lv_area_t area{};lv_obj_get_coords(keyboard,&area);const auto& key=matrix->button_areas[index];
        ClickAt(float(area.x1+(key.x1+key.x2)/2),float(area.y1+(key.y1+key.y2)/2),simulator);return;
    }
    throw std::runtime_error("TFT on-screen keypad digit is missing");
}
}

void CheckTftSettings(UI& editor,SdlSimulator& simulator) {
    SDL_SetModState(SDL_KMOD_NONE);
    editor.ApplyDocument({{"formatVersion",2},{"canvas",{{"width",320},{"height",240}}},{"variables",json::array()},
        {"obj",json::array({{{"oid",0},{"par",-1},{"base",{{"name","Canvas selection"},{"drag",true},{"coords",{{"x",20},{"y",20},{"width",100},{"height",40}}}}},{"button",{{"text","Selected widget"}}}}})}});
    editor.SelectObjectIds({0});Drain(simulator);const auto original=editor.ProjectDocument().at("obj");
    auto* window=OpenSettings(simulator);const auto fields=Fields(window);auto* width=fields[0];auto* height=fields[1];
    auto* submit=ParentButton(Label(window,"Submit Changes"));Require(submit,"TFT submit action is missing");
    auto* group=lv_indev_get_group(simulator.GetKeyboard());Require(group,"Physical keyboard has no editor focus group");
    Click(width,simulator);Require(lv_group_get_focused(group)==width,"Clicking TFT width did not route the physical keyboard");
    Clear(width,simulator);Require(!Ready(submit),"An empty TFT dimension enabled Submit");
    Digits("480",simulator);Require(std::string(lv_textarea_get_text(width))=="480","Top-row key/text input did not enter TFT width exactly once");
    Require(Ready(submit),"Typing a changed valid TFT width did not enable Submit immediately");
    Key(SDLK_LEFT,SDL_SCANCODE_LEFT,simulator);Key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE,simulator);
    Require(std::string(lv_textarea_get_text(width))=="40","Physical Left/Backspace did not edit TFT width");Text("8",simulator);
    Key(SDLK_DELETE,SDL_SCANCODE_DELETE,simulator);Require(std::string(lv_textarea_get_text(width))=="48","Physical Delete did not edit TFT width");Text("0",simulator);
    Require(editor.ProjectDocument().at("obj")==original,"Editing TFT width nudged/deleted the canvas selection");
    Key(SDLK_TAB,SDL_SCANCODE_TAB,simulator);Require(lv_group_get_focused(group)==height,"Tab did not move from TFT width to height");
    Click(height,simulator);Clear(height,simulator);Digits("272",simulator,true);Text("x-.",simulator);
    Require(std::string(lv_textarea_get_text(height))=="272","Numeric keypad input or numeric-only height filtering failed");
    Keypad(FindClass(window,&lv_keyboard_class),"7",simulator);
    Require(std::string(lv_textarea_get_text(height))=="2727","On-screen keypad stopped targeting TFT height");
    Key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE,simulator);
    Require(std::string(lv_textarea_get_text(height))=="272","Physical input did not resume after on-screen keypad use");
    Require(Ready(submit),"Valid TFT dimension edits did not leave Submit available");Click(submit,simulator);
    Require(lv_obj_has_flag(window,LV_OBJ_FLAG_HIDDEN),"Submitting TFT dimensions did not close settings");
    Require(lv_group_get_focused(group)!=width&&lv_group_get_focused(group)!=height,"Submitting left a hidden TFT field focused");
    const auto canvas=editor.ProjectDocument().at("canvas");Require(canvas.at("width")==480&&canvas.at("height")==272,"Submitted TFT dimensions did not resize the authored canvas");
    editor.MenuAction("fit-canvas");Drain(simulator);
    window=OpenSettings(simulator);Click(Fields(window)[0],simulator);
    Click(ParentButton(Label(EditorWindow::Header(window),LV_SYMBOL_CLOSE)),simulator);
    Require(lv_obj_has_flag(window,LV_OBJ_FLAG_HIDDEN),"TFT close button did not hide settings");
    Require(lv_group_get_focused(group)!=width&&lv_group_get_focused(group)!=height,"Closing left hidden TFT fields consuming physical input");
    window=OpenSettings(simulator);const auto deleting=Fields(window);Click(deleting[1],simulator);
    // LVGL shutdown deletes indevs before displays. Removing the binding first
    // reproduces that ordering while keeping the group inspectable afterward.
    lv_indev_set_group(simulator.GetKeyboard(),nullptr);
    lv_obj_delete(window);
    Require(lv_group_get_focused(group)!=deleting[0]&&lv_group_get_focused(group)!=deleting[1],"Deleting focused TFT Settings retained a destroyed numeric field");
    lv_indev_set_group(simulator.GetKeyboard(),group);Drain(simulator);
    window=OpenSettings(simulator);const auto recreated=Fields(window);Click(recreated[1],simulator);
    Clear(recreated[1],simulator);Digits("273",simulator);Key(SDLK_RETURN,SDL_SCANCODE_RETURN,simulator);
    Require(lv_obj_has_flag(window,LV_OBJ_FLAG_HIDDEN),"Physical Enter did not apply recreated TFT Settings");
    Require(editor.ProjectDocument().at("canvas").at("height")==273,"Recreated TFT Settings kept a stale callback binding");
    Require(lv_group_get_focused(group)!=recreated[0]&&lv_group_get_focused(group)!=recreated[1],"Enter left a hidden recreated TFT field focused");
    editor.SelectObjectIds({0});const auto x=editor.ProjectDocument().at("obj").at(0).at("base").at("coords").at("x").get<int>();
    Key(SDLK_RIGHT,SDL_SCANCODE_RIGHT,simulator);
    Require(editor.ProjectDocument().at("obj").at(0).at("base").at("coords").at("x")==x+1,"Canvas arrow shortcuts did not recover after closing TFT Settings");
    Key(SDLK_DELETE,SDL_SCANCODE_DELETE,simulator);Require(editor.ProjectDocument().at("obj").empty(),"Canvas Delete shortcut did not recover after closing TFT Settings");
    std::cout<<"TFT physical digits/numpad, text input, cursor/Delete/Backspace, Tab, numeric filtering, live Submit, on-screen keypad and canvas focus recovery passed\n";
}
