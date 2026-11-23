#include "DevicePlayground.h"
#include "../../../UICreator/Devices/DeviceWindow.h"
#include "../../../UICreator/LanguageModels/ModelPlayground.h"
#include "../../../UICreator/LanguageModels/Generator.h"
#include "../../../UICreator/Utils/EditorSession.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Windows/UI.h"
#include <lv_advanced_keyboard.h>
#include <SDL3/SDL.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
lv_obj_t* Label(lv_obj_t* root,const char* text){if(lv_obj_has_class(root,&lv_label_class)&&std::string(lv_label_get_text(root))==text)return root;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* value=Label(lv_obj_get_child(root,i),text))return value;return nullptr;}
bool HasText(lv_obj_t* root,const char* text){if(lv_obj_has_class(root,&lv_label_class)&&std::string(lv_label_get_text(root)).find(text)!=std::string::npos)return true;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(HasText(lv_obj_get_child(root,i),text))return true;return false;}
lv_obj_t* First(lv_obj_t* root,const lv_obj_class_t* type){if(lv_obj_has_class(root,type))return root;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* value=First(lv_obj_get_child(root,i),type))return value;return nullptr;}
lv_obj_t* Field(lv_obj_t* root,const char* name){auto* label=Label(root,name);Require(label,"Device tool input label missing");return lv_obj_get_child(lv_obj_get_parent(label),1);}
void Click(lv_obj_t* root,const char* text){auto* label=Label(root,text);Require(label,"Device/playground action missing");auto* button=lv_obj_get_parent(label);Require(lv_obj_has_class(button,&lv_button_class)&&!lv_obj_has_state(button,LV_STATE_DISABLED),"Device/playground action unavailable");lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);}
void Frames(SdlSimulator& simulator,unsigned milliseconds=100){const auto end=SDL_GetTicks()+milliseconds;while(SDL_GetTicks()<end)simulator.ProcessEvents();}
void Wait(SdlSimulator& simulator){const auto end=SDL_GetTicks()+15000;while(DeviceWindow::Running()){Require(SDL_GetTicks()<end,"Device tool worker failed to finish");simulator.ProcessEvents();}Frames(simulator);}
}
void CheckDevicePlayground(UI& editor,SdlSimulator& simulator){
    namespace fs=std::filesystem;
    const auto work=fs::absolute("device-playground-work")/("run-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(work);
    const auto session=work/"session";
#ifdef _WIN32
    Require(SetEnvironmentVariableW(L"UICREATOR_SESSION_DIR",session.c_str())!=0,"Cannot isolate device session settings");
#else
    Require(setenv("UICREATOR_SESSION_DIR",session.c_str(),1)==0,"Cannot isolate device session settings");
#endif
    auto* trayTitle=Label(lv_screen_active(),"Tool Tray");Require(trayTitle,"Tool Tray missing");auto* palette=EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(trayTitle)));
    Click(palette,"Text area");editor.MenuAction("select-screen");Click(palette,"Advanced\nWidgets");Click(palette,"Advanced Keyboard");
    auto* simTitle=Label(lv_screen_active(),"Simulator [Size:320x240]");Require(simTitle,"Simulator missing");auto* surface=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(simTitle))),0);auto* keyboard=lv_obj_get_child(surface,1);Require(lv_advanced_keyboard_is_instance(keyboard),"Cannot create test keyboard");auto* properties=static_cast<ObjectUserData*>(lv_obj_get_user_data(keyboard))->toolTray->GetPropertyWindow();
    {std::ofstream corpus(work/"input.txt",std::ios::binary);corpus<<"hello world. hello world. hello there. help the hero. hello there.\n";}
    LanguageModels::Options options;options.inputs={work/"input.txt"};options.trieOutput=work/"project-model.ngtr";options.maximumBytes.reset();options.useWordList=false;options.useCache=false;LanguageModels::Generate(options);
    std::string projectModelId;
    {EditorEdit::Scope edit;projectModelId=properties->Tries().Import(options.trieOutput);}properties->SetSelectedObject(keyboard);const auto before=editor.ProjectDocument();
    editor.MenuAction("model-playground");auto* playground=ModelPlayground::ActiveWindow();Require(playground,"Playground menu did not open a native window");Require(Label(playground,"Model A")&&Label(playground,"Compare model B"),"Two model comparison panels missing");auto* sample=First(playground,&lv_textarea_class);Require(sample,"Playground sample text missing");
    lv_textarea_set_text(sample,"teh");lv_textarea_set_cursor_pos(sample,LV_TEXTAREA_CURSOR_LAST);std::unique_ptr<lv_advanced_keyboard_predictor_t,decltype(&lv_advanced_keyboard_predictor_destroy)> builtin(lv_advanced_keyboard_predictor_create_builtin(),lv_advanced_keyboard_predictor_destroy);Require(bool(builtin),"Builtin predictor unavailable");auto correction=ModelPlayground::Evaluate(sample,builtin.get(),.3);Require(correction.correction=="the"&&correction.accepted&&correction.confidence>.3,"Playground does not use working builtin autocorrect");auto strict=ModelPlayground::Evaluate(sample,builtin.get(),1);Require(strict.correction=="the"&&!strict.accepted,"Playground threshold comparison is incorrect");
    lv_textarea_set_text(sample,"was in hel");lv_textarea_set_cursor_pos(sample,LV_TEXTAREA_CURSOR_LAST);auto context=ModelPlayground::Evaluate(sample,builtin.get(),.3);Require(context.prefix=="hel"&&context.previous=="in"&&context.older=="was"&&!context.suggestions.empty(),"Playground lost actual caret context or completions");
    auto* bTitle=Label(playground,"Compare model B");auto* bPanel=lv_obj_get_parent(bTitle);auto* bChoice=First(bPanel,&lv_dropdown_class);Require(lv_dropdown_get_option_count(bChoice)==before["tries"].size()+2,"Project trie missing from playground choices");
    uint32_t projectChoice=2;for(const auto& model:before["tries"]){if(model.at("id")==projectModelId)break;++projectChoice;}
    lv_dropdown_set_selected(bChoice,projectChoice);lv_obj_send_event(bChoice,LV_EVENT_VALUE_CHANGED,nullptr);Frames(simulator,160);Require(HasText(bPanel,"hello")&&HasText(bPanel,"workspace "),"Project model did not produce real suggestions and memory metrics");Require(editor.ProjectDocument()==before,"Trying models or typing changed the project");
    Click(bPanel,"Load into project");Require(editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]==projectModelId,"Explicit playground load did not assign the selected keyboard");Require(editor.ProjectDocument()["tries"]==before["tries"],"Choosing an existing project model duplicated its bytes");ModelPlayground::Close();Require(!ModelPlayground::ActiveWindow(),"Playground close leaked its window");Require(editor.Undo(),"Playground assignment was not undoable");Require(editor.ProjectDocument()==before,"Undo did not restore the original project model assignment");keyboard=lv_obj_get_child(surface,1);properties->SetSelectedObject(keyboard);
    editor.MenuAction("device-tools");auto* device=DeviceWindow::ActiveWindow();Require(device,"Device menu did not open a native tool window");Require(Label(device,"Configure")&&Label(device,"Build output")&&Label(device,"Serial monitor"),"Device workflow tabs missing");auto* profile=Field(device,"Toolchain");lv_dropdown_set_selected(profile,2);lv_obj_send_event(profile,LV_EVENT_VALUE_CHANGED,nullptr);lv_textarea_set_text(Field(device,"Project folder"),work.u8string().c_str());lv_textarea_set_text(Field(device,"Serial port"),"");Require(lv_obj_has_state(lv_obj_get_parent(Label(device,"Flash board")),LV_STATE_DISABLED),"CMake profile offers a generic hardware flash");
    Click(device,"Tool version");Require(DeviceWindow::Running(),"Version probe did not start background work");Wait(simulator);Require(HasText(device,"cmake version")&&HasText(device,"Completed successfully"),"Device version probe did not stream actual tool output");Require(editor.ProjectDocument()==before,"Device version probe modified project contents");
    DeviceWindow::Close();Require(!DeviceWindow::ActiveWindow()&&!DeviceWindow::Running(),"Device close leaked workers or window");Require(fs::is_regular_file(session/"device-tools.json"),"Device settings were not saved in the isolated session");editor.MenuAction("device-tools");device=DeviceWindow::ActiveWindow();Require(lv_dropdown_get_selected(Field(device,"Toolchain"))==2&&std::string(lv_textarea_get_text(Field(device,"Project folder")))==work.u8string(),"Device profile settings did not persist");
    Click(device,"Tool version");DeviceWindow::Close();Require(!DeviceWindow::Running(),"Closing an active tool left its worker running");
    editor.MenuAction("model-playground");editor.MenuAction("device-tools");editor.ApplyDocument(editor.ProjectDocument());Require(!ModelPlayground::ActiveWindow()&&!DeviceWindow::ActiveWindow(),"Project replacement retained tool windows with stale project pointers");Frames(simulator);
    std::cout<<"Device tool lifecycle/settings/native version streaming and model playground context/comparison/load/undo passed\n";
}
