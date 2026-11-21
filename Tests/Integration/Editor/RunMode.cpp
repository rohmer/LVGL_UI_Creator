#include "RunMode.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include <Serialization/ProjectDesign.h>
#include <stdexcept>
#include <iostream>

namespace {
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void Pump(SdlSimulator& simulator,int frames=12){for(int i=0;i<frames;++i){SDL_Delay(6);simulator.ProcessEvents();}}
void Pointer(UI& ui,SdlSimulator& simulator,int x,int y) {
    lv_obj_update_layout(lv_layer_top());lv_area_t bounds{};lv_obj_get_coords(ui.RunImage(),&bounds);
    const auto window=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    SDL_Event move{};move.type=SDL_EVENT_MOUSE_MOTION;move.motion.windowID=window;move.motion.x=float(bounds.x1+x);move.motion.y=float(bounds.y1+y);SDL_PushEvent(&move);
    SDL_Event button{};button.type=SDL_EVENT_MOUSE_BUTTON_DOWN;button.button.windowID=window;button.button.button=SDL_BUTTON_LEFT;button.button.down=true;button.button.x=move.motion.x;button.button.y=move.motion.y;SDL_PushEvent(&button);Pump(simulator);
    button.type=SDL_EVENT_MOUSE_BUTTON_UP;button.button.down=false;SDL_PushEvent(&button);Pump(simulator);
}
}
void CheckRunMode(UI& editor,SdlSimulator& simulator) {
    auto document=editor.ProjectDocument();document["canvas"]["width"]=480;document["canvas"]["height"]=320;
    document["obj"]=json::array({
        {{"oid",0},{"par",-1},{"base",{{"name","Entry"},{"coords",{{"x",20},{"y",20},{"width",280},{"height",50}}}}},{"textarea",{{"text",""},{"oneLine",true}}}},
        {{"oid",1},{"par",-1},{"base",{{"name","Run action"},{"coords",{{"x",20},{"y",100},{"width",180},{"height",55}}},{"events",json::array({{{"event","clicked"},{"action","increment_variable"},{"variable","clicks"},{"value",1}},{{"event","clicked"},{"action","hide"},{"targetOid",2}}})}}},{"button",{{"text","Apply"}}}},
        {{"oid",2},{"par",-1},{"base",{{"name","Status"},{"coords",{{"x",20},{"y",190},{"width",240},{"height",30}}}}},{"text","Ready"}}
    });
    document["variables"]=json::array({{{"var",{{"name","clicks"},{"dataType",1},{"default",0},{"oid",-1},{"property",""},{"history",4}}}}});
    Serialization::ProjectDesign::ApplyTheme(document,"ocean-dark");editor.ApplyDocument(document);
    const auto authored=editor.ProjectDocument().dump(); auto* display=lv_display_get_default();auto* screen=lv_screen_active();auto* group=lv_group_get_default();
    editor.MenuAction("run-toggle");Require(editor.Running(),"Run mode did not start portable runtime");Pump(simulator);
    Require(lv_display_get_default()==display && lv_screen_active()==screen && lv_group_get_default()==group,"Run mode replaced editor display or focus group");
    auto* runtime=editor.RunRuntime();auto* entry=lvgl_runtime_object(runtime,0,0);auto* status=lvgl_runtime_object(runtime,0,2);
    Require(entry && status && lv_obj_get_display(entry)!=display,"Run widgets were not isolated");
    Pointer(editor,simulator,60,40);
    SDL_Event input{};input.type=SDL_EVENT_TEXT_INPUT;input.text.text="Hello";SDL_PushEvent(&input);Pump(simulator);
    Require(std::string(lv_textarea_get_text(entry))=="Hello","Native keyboard typing was not routed to Run textarea");
    Pointer(editor,simulator,80,125);
    lvgl_runtime_value_t count{};Require(lvgl_runtime_get_variable(runtime,"clicks",&count) && count.integer_value==1,"Run pointer click did not execute variable action");
    Require(lv_obj_is_hidden(status),"Run action did not hide target widget");
    Require(editor.ProjectDocument().dump()==authored,"Run interactions modified authored project");
    Require(editor.HandleShortcut(SDLK_ESCAPE,false),"Escape did not leave Run mode");Require(!editor.Running(),"Run mode remained active");
    Require(editor.ProjectDocument().dump()==authored,"Leaving Run changed authored project");
    for(int i=0;i<3;++i) {
        Require(editor.HandleShortcut(SDLK_F6,false),"F6 did not start Run");Require(editor.Running(),"Repeated Run failed");
        Require(lvgl_runtime_get_variable(editor.RunRuntime(),"clicks",&count) && count.integer_value==0,"Restart retained runtime variable mutations");
        editor.MenuAction("run-stop");
    }
    Require(editor.Undo(),"Theme/document transaction was not undoable");Require(!editor.ProjectDocument().contains("projectTheme"),"Undo lost prior project metadata");
    Require(editor.Redo(),"Redo did not restore document");
    if(editor.ProjectDocument().dump()!=authored)throw std::runtime_error("Redo did not restore theme/events/document: "+json::diff(json::parse(authored),editor.ProjectDocument()).dump());
    std::cout<<"Run isolation, portable actions, pointer and keyboard routing, restart, themes and undo passed\n";
    editor.MenuAction("run-toggle");Pump(simulator);
}
