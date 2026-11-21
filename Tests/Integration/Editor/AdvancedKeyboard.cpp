#include "AdvancedKeyboard.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Windows/PropertyWindows/AdvancedProperties.h"
#include <lv_advanced_keyboard.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
lv_obj_t* Label(lv_obj_t* parent,const char* text) {
    if(lv_obj_get_class(parent)==&lv_label_class && std::string(lv_label_get_text(parent))==text)return parent;
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i)if(auto* found=Label(lv_obj_get_child(parent,i),text))return found;
    return nullptr;
}
lv_obj_t* Field(PropertyWindow* properties,const char* name) {
    auto* label=Label(properties->ObjectPropWin()->GetWindow(),name);Require(label,"Keyboard inspector field missing");
    return lv_obj_get_child(lv_obj_get_parent(label),1);
}
void Edit(lv_obj_t* field,const char* text) {lv_textarea_set_text(field,text);lv_obj_send_event(field,LV_EVENT_VALUE_CHANGED,nullptr);}
}
void CheckAdvancedKeyboardEditor(UI& editor,SdlSimulator& simulator) {
    
    auto* trayTitle=Label(lv_screen_active(),"Tool Tray");Require(trayTitle,"Tool Tray missing");
    auto* palette=EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(trayTitle)));
    auto click=[&](const char* name) {auto* label=Label(palette,name);Require(label,"Palette entry missing");lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);lv_obj_update_layout(lv_screen_active());};
    click("Text area");editor.MenuAction("select-screen");click("Advanced\nWidgets");
    click("Advanced Keyboard");
    auto* title=Label(lv_screen_active(),"Simulator [Size:320x240]");Require(title,"Simulator missing");
    auto* surface=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))),0);
    auto* entry=lv_obj_get_child(surface,0);auto* keyboard=lv_obj_get_child(surface,1);
    Require(lv_obj_has_class(entry,&lv_textarea_class) && lv_advanced_keyboard_is_instance(keyboard),"External text field/keyboard creation failed");
    Require(lv_obj_get_child_count(keyboard)==2,"Keyboard still owns a text field");
    auto properties=[&]{return static_cast<ObjectUserData*>(lv_obj_get_user_data(keyboard))->toolTray->GetPropertyWindow();};
    Require(Label(properties()->ObjectPropWin()->GetWindow(),"Language") && Label(properties()->ObjectPropWin()->GetWindow(),"Load trie...") &&
        Label(properties()->ObjectPropWin()->GetWindow(),"Prediction trie") && Label(properties()->ObjectPropWin()->GetWindow(),"Target text box"),"Language/trie/target controls missing");
    const auto beforeFocus=editor.ProjectDocument();properties()->SetSelectedObject(entry);
    Require(lv_advanced_keyboard_get_textarea(keyboard)==entry && !lv_obj_has_flag(keyboard,LV_OBJ_FLAG_HIDDEN),"Selecting an external text field did not show/target the keyboard");
    Require(editor.ProjectDocument()==beforeFocus,"Focus visibility changed the saved document");
    auto* key=Label(lv_advanced_keyboard_get_keyboard(keyboard),"q");Require(key,"Virtual typing key missing");
    lv_obj_send_event(lv_obj_get_parent(key),LV_EVENT_CLICKED,nullptr);
    Require(properties()->GetSelectedObject()==entry && lv_group_get_focused(properties()->GetKBGroup())==entry,
        "A virtual key stole selection or physical keyboard focus from the text field");
    properties()->SetSelectedObject(keyboard);Edit(Field(properties(),"Key Background"),"#223344");
    Require(editor.Undo(),"Keyboard style edit was not undoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(editor.Redo(),"Keyboard style edit was not redoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["keyBackground"]==0xff223344u,"Redo lost keyboard settings");
    properties()->SetSelectedObject(keyboard);
    auto selectLanguage=[&](const char* id) {
        auto* control=Field(properties(),"Language");Require(lv_obj_has_class(control,&lv_dropdown_class),"Language selector is not a dropdown");
        size_t index=0;while(index<lv_advanced_keyboard_language_count() && std::string(lv_advanced_keyboard_language_id(index))!=id)++index;
        Require(index<lv_advanced_keyboard_language_count(),"Expected keyboard language is missing");
        lv_dropdown_set_selected(control,static_cast<uint32_t>(index));lv_obj_send_event(control,LV_EVENT_VALUE_CHANGED,nullptr);
    };
    Require(lv_dropdown_get_option_count(Field(properties(),"Language"))==6 && std::string(lv_advanced_keyboard_get_language(keyboard))=="en-US","Language defaults/list are incorrect");
    selectLanguage("es-ES");
    Require(std::string(lv_advanced_keyboard_get_language(keyboard))=="es-ES" && !lv_advanced_keyboard_get_predictor(keyboard),"Spanish selection did not change native layout or retained the English dictionary");
    Require(editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["language"]=="es-ES" && lv_dropdown_get_selected(Field(properties(),"Prediction trie"))==0,"Language/trie selection was not persisted or refreshed");
    Require(editor.Undo(),"Keyboard language change was not undoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(std::string(lv_advanced_keyboard_get_language(keyboard))=="en-US" && lv_advanced_keyboard_get_predictor(keyboard),"Undo lost the English language/model");
    Require(editor.Redo(),"Keyboard language change was not redoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(std::string(lv_advanced_keyboard_get_language(keyboard))=="es-ES" && !lv_advanced_keyboard_get_predictor(keyboard),"Redo lost the selected language/model");
    properties()->SetSelectedObject(keyboard);Edit(Field(properties(),"Autocorrect Threshold"),"0.7");
    const uint8_t trie[]={'N','G','T','R',3,0,0,0,4,0,0,0,0,0,0,0,0,2,0,2,'a',1,1,'a',1,1};
    auto path=std::filesystem::temp_directory_path()/("creator-trie-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ngtr");
    {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(trie),sizeof trie);}
    try {AdvancedProperties::LoadTrie(properties(),path);AdvancedProperties::LoadTrie(properties(),path);}
    catch(...){std::filesystem::remove(path);throw;}std::filesystem::remove(path);
    const auto imported=editor.ProjectDocument();Require(imported["tries"].size()==1,"Trie import did not content-deduplicate");
    const auto& settings=imported["obj"][1]["advancedKeyboard"];
    Require(settings["trieId"]==imported["tries"][0]["id"] && settings["autocorrectThreshold"]==0.7,"Selected trie/settings were not saved");
    selectLanguage("fr-FR");
    Require(std::string(lv_advanced_keyboard_get_language(keyboard))=="fr-FR" && lv_advanced_keyboard_get_predictor(keyboard) &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]==imported["tries"][0]["id"],"Language change discarded an explicitly loaded trie");
    Require(!settings.contains("text") && !settings.contains("password"),"Keyboard retained private text field properties");
    auto selectTrie=[&](uint32_t index) {
        auto* control=Field(properties(),"Prediction trie");Require(lv_obj_has_class(control,&lv_dropdown_class),"Trie selector is not a dropdown");
        lv_dropdown_set_selected(control,index);lv_obj_send_event(control,LV_EVENT_VALUE_CHANGED,nullptr);
    };
    selectTrie(1);Require(lv_advanced_keyboard_get_predictor(keyboard),"Bundled trie selection did not attach a predictor");
    selectTrie(0);Require(!lv_advanced_keyboard_get_predictor(keyboard),"None trie selection kept its predictor");selectTrie(2);
    auto* target=Field(properties(),"Target text box");
    Require(lv_obj_has_class(target,&lv_dropdown_class),"Target text box inspector control is not a dropdown");
    lv_dropdown_set_selected(target,1);lv_obj_send_event(target,LV_EVENT_VALUE_CHANGED,nullptr);
    Require(editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["targetOid"]==0,"Target selector did not persist the external field's serialized object ID");
    properties()->SetSelection({entry,keyboard});editor.MenuAction("duplicate");
    auto clones=editor.ProjectDocument();Require(clones["obj"].size()==4 && clones["obj"][3]["advancedKeyboard"]["targetOid"]==2,
        "Duplicating a field and keyboard did not link the cloned pair");
    Require(editor.Undo(),"Duplicated field pair was not undoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    properties()->SetSelectedObject(entry);editor.MenuAction("delete");
    auto removed=editor.ProjectDocument();Require(removed["obj"].size()==1 && removed["obj"][0]["advancedKeyboard"]["targetOid"]==-1,
        "Deleting the target did not clear the keyboard association");
    Require(editor.Undo(),"Target deletion was not undoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    properties()->SetSelectedObject(keyboard);
    const auto valid=editor.ProjectDocument();Edit(Field(properties(),"Autocorrect Threshold"),"1.5");Require(editor.ProjectDocument()==valid,"Invalid threshold changed the project");
    editor.MenuAction("export-c");editor.MenuAction("export-preflight");bool ready=false;
    auto inspect=[&](auto&& self,lv_obj_t* object)->void {if(lv_obj_get_class(object)==&lv_label_class && std::string(lv_label_get_text(object)).find("Ready for LVGL 9.6")==0)ready=true;for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)self(self,lv_obj_get_child(object,i));};
    inspect(inspect,lv_layer_top());Require(ready,"Trie/target C export preflight failed");editor.MenuAction("cancel-file");
    properties()->SetSelectedObject(entry);for(int frame=0;frame<5;++frame)simulator.ProcessEvents();
    std::cout<<"Keyboard external target, focus visibility, language selection, trie loading, persistence, undo/redo and C export passed\n";
}
