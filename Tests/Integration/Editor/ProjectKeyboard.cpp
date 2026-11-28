#include "ProjectKeyboard.h"
#include "EditorInputTest.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Windows/PropertyWindows/ProjectKeyboardProperties.h"
#include "../../../UICreator/KeyboardModels/BundledTries.h"
#include <Serialization/Protobuf.h>
#include <lv_advanced_keyboard.h>
#include <chrono>
#include <fstream>
#include <iostream>

namespace {
namespace Input = EditorInputTest;
lv_obj_t* Checkbox(lv_obj_t* root) {
    if(lv_obj_has_class(root,&lv_checkbox_class) && std::string(lv_checkbox_get_text(root))=="Enable Advanced Keyboard")return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=Checkbox(lv_obj_get_child(root,i)))return found;
    return nullptr;
}
lv_obj_t* Keyboard(lv_obj_t* root) {
    if(lv_advanced_keyboard_is_instance(root))return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=Keyboard(lv_obj_get_child(root,i)))return found;
    return nullptr;
}
void Pointer(UI& editor,SdlSimulator& simulator,lv_obj_t* target) {
    lv_obj_update_layout(lv_obj_get_screen(target));lv_obj_update_layout(lv_layer_top());
    lv_area_t object{},image{};lv_obj_get_coords(target,&object);lv_obj_get_coords(editor.RunImage(),&image);
    const auto window=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=window;
    motion.motion.x=float(image.x1+(object.x1+object.x2)/2);motion.motion.y=float(image.y1+(object.y1+object.y2)/2);SDL_PushEvent(&motion);
    SDL_Event button{};button.type=SDL_EVENT_MOUSE_BUTTON_DOWN;button.button.windowID=window;
    button.button.button=SDL_BUTTON_LEFT;button.button.down=true;button.button.x=motion.motion.x;button.button.y=motion.motion.y;
    SDL_PushEvent(&button);Input::Drain(simulator);
    button.type=SDL_EVENT_MOUSE_BUTTON_UP;button.button.down=false;SDL_PushEvent(&button);Input::Drain(simulator);
}
void Key(lv_obj_t* keyboard,const char* caption) {
    auto* label=Input::FindLabel(lv_advanced_keyboard_get_keyboard(keyboard),caption);
    Input::Require(label,std::string("Missing keyboard key: ")+caption);
    lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);
}
bool Memory(lv_obj_t* root,size_t& bytes) {
    if(lv_obj_has_class(root,&lv_label_class)) {
        const std::string text=lv_label_get_text(root),prefix="Screen Memory: ";
        if(text.rfind(prefix,0)==0){bytes=std::stoull(text.substr(prefix.size()));return true;}
    }
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(Memory(lv_obj_get_child(root,i),bytes))return true;
    return false;
}
}

void CheckProjectKeyboardEditor(UI& editor,SdlSimulator& simulator) {
    json document={{"formatVersion",2},{"lvglVersion","9.6.0"},{"canvas",{{"width",480},{"height",320}}},
        {"obj",json::array({
            {{"oid",0},{"par",-1},{"base",{{"name","Name"},{"coords",{{"x",20},{"y",20},{"width",280},{"height",48}}}}},{"textarea",{{"text",""},{"oneLine",true}}}},
            {{"oid",1},{"par",-1},{"base",{{"name","Number"},{"coords",{{"x",20},{"y",80},{"width",280},{"height",48}}}}},{"textarea",{{"text",""},{"oneLine",true},{"inputMode","numeric"}}}}
        })}};
    editor.ApplyDocument(document);Input::Drain(simulator);
    auto* canvasWindow=Input::WindowFromTitle("Simulator [Size:480x320]");Input::Require(canvasWindow,"Design canvas missing");
    auto* canvas=lv_obj_get_child(EditorWindow::Content(canvasWindow),0);
    auto* properties=static_cast<ObjectUserData*>(lv_obj_get_user_data(lv_obj_get_child(canvas,0)))->toolTray->GetPropertyWindow();
    auto* tray=Input::WindowFromTitle("Tool Tray");Input::Require(tray && !Input::FindLabel(tray,"Advanced Keyboard"),"Advanced Keyboard is still a manual palette widget");
    auto* navigator=Input::WindowFromTitle("Screens");auto* project=Input::FindLabel(EditorWindow::Content(navigator),"Project");
    Input::Require(project,"Project entry above Screens is missing");
    lv_obj_send_event(lv_obj_get_parent(project),LV_EVENT_CLICKED,nullptr);Input::Drain(simulator);
    Input::Require(properties->ShowingProjectProperties(),"Project navigation did not open project properties");
    auto toggle=[&](bool enabled) {
        auto* control=Checkbox(properties->ObjectPropWin()->GetWindow());Input::Require(control,"Project enable checkbox missing");
        lv_obj_set_state(control,LV_STATE_CHECKED,enabled);lv_obj_send_event(control,LV_EVENT_VALUE_CHANGED,nullptr);Input::Drain(simulator);
    };
    size_t beforeMemory=0;Input::Require(Memory(canvasWindow,beforeMemory),"Screen memory label missing");
    toggle(true);
    for(int i=0;i<20;++i){SDL_Delay(6);simulator.ProcessEvents();}
    size_t enabledMemory=0;Input::Require(Memory(canvasWindow,enabledMemory) && enabledMemory>beforeMemory,"Screen Memory excluded the enabled runtime keyboard");
    Input::Require(editor.ProjectDocument()["advancedKeyboard"]["enabled"]==true && !Keyboard(canvas),"Enabling project keyboard added an authoring widget or lost configuration");
    Input::Require(editor.Undo() && !editor.ProjectDocument().value("advancedKeyboard",json::object()).value("enabled",false),"Project keyboard enable is not undoable");
    Input::Require(properties->ShowingProjectProperties() && editor.Redo() && properties->ShowingProjectProperties(),"Undo/redo lost project property selection");
    auto* language=Input::FindField(properties->ObjectPropWin()->GetWindow(),"Language",&lv_dropdown_class);
    Input::Require(language,"Project language selector missing");
    uint32_t french=0;for(size_t i=0;i<lv_advanced_keyboard_language_count();++i)if(std::string(lv_advanced_keyboard_language_id(i))=="fr-FR")french=uint32_t(i);
    lv_dropdown_set_selected(language,french);lv_obj_send_event(language,LV_EVENT_VALUE_CHANGED,nullptr);Input::Drain(simulator);
    auto configured=editor.ProjectDocument();
    Input::Require(configured["advancedKeyboard"]["language"]=="fr-FR" && configured["advancedKeyboard"]["trieSizeKiB"]==16 && !configured["tries"].empty(),"Project language did not embed its bundled trie");
    const auto* model=BundledTries::Find("fr-FR",16);Input::Require(model,"French model fixture missing");
    const auto bytes=BundledTries::Read(*model);
    const auto path=std::filesystem::temp_directory_path()/("creator-project-keyboard-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ngtr");
    {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));}
    try {ProjectKeyboardProperties::LoadTrie(properties,path);}catch(...){std::filesystem::remove(path);throw;}
    std::filesystem::remove(path);Input::Drain(simulator);
    auto* threshold=Input::FindField(properties->ObjectPropWin()->GetWindow(),"Autocorrect threshold (0-1)");
    Input::Require(threshold,"Project autocorrect threshold field missing");
    Input::Replace(threshold,"0.7",simulator);
    Input::Require(editor.ProjectDocument()["advancedKeyboard"]["autocorrectThreshold"]==0.7,"Physical typing did not update project keyboard options");
    configured=editor.ProjectDocument();const auto settings=configured.at("advancedKeyboard");
    Input::Require(settings["trieSizeKiB"]==0 && !settings.at("trieId").get<std::string>().empty(),"Project custom trie was not saved");
    toggle(false);Input::Require(editor.ProjectDocument()["advancedKeyboard"]["trieId"]==settings["trieId"],"Disabling discarded selected trie");
    toggle(true);
    const auto second=editor.AddScreen("Second");Input::Require(second==1,"Cannot add a second screen");
    auto multiple=editor.ProjectDocument();multiple["obj"]=document["obj"];editor.ApplyDocument(multiple);
    Input::Require(editor.ProjectDocument()["advancedKeyboard"]["trieId"]==settings["trieId"],"Adding a screen discarded project keyboard settings");
    for(size_t screen=0;screen<2;++screen) {
        Input::Require(editor.SelectScreen(screen),"Cannot select keyboard test screen");
        const auto authored=editor.ProjectDocument();
        Input::Require(editor.HandleShortcut(SDLK_F6,false) && editor.Running(),"F6 did not start project keyboard preview");Input::Drain(simulator);
        auto* runtime=editor.RunRuntime();auto* keyboard=Keyboard(lvgl_runtime_screen(runtime,screen));
        auto* entry=lvgl_runtime_object(runtime,screen,0);auto* numeric=lvgl_runtime_object(runtime,screen,1);
        Input::Require(keyboard && entry && numeric && lv_obj_is_hidden(keyboard),"Project keyboard was missing or opened before editing");
        Input::Require(std::string(lv_advanced_keyboard_get_language(keyboard))=="fr-FR" && lv_advanced_keyboard_get_predictor(keyboard),"Global language/trie was not applied on every screen");
        Pointer(editor,simulator,entry);
        Input::Require(!lv_obj_is_hidden(keyboard) && lv_advanced_keyboard_get_textarea(keyboard)==entry,"Clicking text field did not show the project keyboard");
        Key(keyboard,"a");Input::Require(std::string(lv_textarea_get_text(entry))=="a","Project virtual key did not enter text");
        Pointer(editor,simulator,numeric);
        Input::Require(lv_advanced_keyboard_get_textarea(keyboard)==numeric && !Input::FindLabel(lv_advanced_keyboard_get_keyboard(keyboard),"a"),"Switching targets did not select numeric keyboard");
        Key(keyboard,"7");Input::Require(std::string(lv_textarea_get_text(numeric))=="7","Numeric target did not accept keypad input");
        lv_advanced_keyboard_close(keyboard);Input::Require(lv_obj_is_hidden(keyboard),"Project keyboard did not dismiss");
        Pointer(editor,simulator,numeric);Input::Require(!lv_obj_is_hidden(keyboard),"Project keyboard did not reopen on same field");
        editor.MenuAction("run-stop");Input::Drain(simulator);
        Input::Require(editor.ProjectDocument()==authored,"Run keyboard modified the authored project");
    }
    editor.MenuAction("project-settings");toggle(false);
    Input::Require(editor.HandleShortcut(SDLK_F6,false) && editor.Running(),"Disabled keyboard preview failed");Input::Drain(simulator);
    Input::Require(!Keyboard(lvgl_runtime_screen(editor.RunRuntime(),editor.ActiveScreenIndex())),"Disabled project still created a keyboard");
    editor.MenuAction("run-stop");editor.MenuAction("project-settings");toggle(true);
    const auto roundtrip=Serialization::Protobuf::Decode(Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",editor.ProjectDocument())).data;
    Input::Require(roundtrip.at("advancedKeyboard")==editor.ProjectDocument().at("advancedKeyboard"),"Protobuf lost global keyboard options");
    editor.ApplyDocument(roundtrip);editor.MenuAction("project-settings");Input::Drain(simulator);
    Input::Require(properties->ShowingProjectProperties() && editor.ProjectDocument()["advancedKeyboard"]["enabled"]==true,"Reload lost project keyboard configuration");
    std::cout<<"Project keyboard: palette removal, global options, trie import, undo/redo, two-screen F6, initial hiding, numeric switching, dismissal/reopen and persistence passed\n";
}
