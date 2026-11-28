#include "AdvancedKeyboard.h"
#include "EditorInputTest.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Windows/PropertyWindows/AdvancedProperties.h"
#include <lv_advanced_keyboard.h>
#include <lv_advanced_keyboard_prediction.h>
#include <Serialization/Protobuf.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
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
lv_obj_t* Checkbox(lv_obj_t* parent,const char* text) {
    if(lv_obj_has_class(parent,&lv_checkbox_class)&&std::string(lv_checkbox_get_text(parent))==text)return parent;
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i)if(auto* found=Checkbox(lv_obj_get_child(parent,i),text))return found;
    return nullptr;
}
}
void CheckAdvancedKeyboardEditor(UI& editor,SdlSimulator& simulator) {
    
    auto* trayTitle=Label(lv_screen_active(),"Tool Tray");Require(trayTitle,"Tool Tray missing");
    auto* palette=EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(trayTitle)));
    auto click=[&](const char* name) {auto* label=Label(palette,name);Require(label,"Palette entry missing");lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);lv_obj_update_layout(lv_screen_active());};
    click("Text area");
    // Old project files can still contain a manually authored keyboard even
    // though new screens configure their keyboard in Screen properties.
    auto legacy=editor.ProjectDocument();
    legacy["obj"].push_back({{"oid",1},{"par",-1},{"base",{{"name","Legacy keyboard"},
        {"coords",{{"x",0},{"y",96},{"width",300},{"height",144}}}}},{"advancedKeyboard",json::object()}});
    editor.ApplyDocument(std::move(legacy));
    auto* title=Label(lv_screen_active(),"Simulator [Size:320x240]");Require(title,"Simulator missing");
    auto* surface=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))),0);
    auto* entry=lv_obj_get_child(surface,0);auto* keyboard=lv_obj_get_child(surface,1);
    Require(lv_obj_has_class(entry,&lv_textarea_class) && lv_advanced_keyboard_is_instance(keyboard),"External text field/keyboard creation failed");
    Require(lv_obj_get_child_count(keyboard)==2,"Keyboard still owns a text field");
    auto properties=[&]{return static_cast<ObjectUserData*>(lv_obj_get_user_data(keyboard))->toolTray->GetPropertyWindow();};
    properties()->Tries().ApplyDefault(keyboard);
    properties()->SetSelectedObject(keyboard);
    Require(Label(properties()->ObjectPropWin()->GetWindow(),"Language") && Label(properties()->ObjectPropWin()->GetWindow(),"Load trie...") &&
        Label(properties()->ObjectPropWin()->GetWindow(),"Trie size") && Label(properties()->ObjectPropWin()->GetWindow(),"Prediction trie") && Label(properties()->ObjectPropWin()->GetWindow(),"Target text box"),"Language/size/trie/target controls missing");
    const auto englishDefault=editor.ProjectDocument();
    Require(englishDefault["obj"][1]["advancedKeyboard"].value("trieSizeKiB",0u)==16 && englishDefault["tries"].size()==1 &&
        englishDefault["obj"][1]["advancedKeyboard"]["trieId"]==englishDefault["tries"][0]["id"],"New keyboard did not embed its default English corpus model");
    const auto beforeFocus=editor.ProjectDocument();properties()->SetSelectedObject(entry);
    Require(lv_advanced_keyboard_get_textarea(keyboard)==entry && !lv_obj_is_hidden(keyboard),"Selecting an external text field did not show/target the keyboard");
    Require(editor.ProjectDocument()==beforeFocus,"Focus visibility changed the saved document");
    auto* key=Label(lv_advanced_keyboard_get_keyboard(keyboard),"q");Require(key,"Virtual typing key missing");
    lv_obj_send_event(lv_obj_get_parent(key),LV_EVENT_CLICKED,nullptr);
    Require(properties()->GetSelectedObject()==entry && lv_group_get_focused(properties()->GetKBGroup())==entry,
        "A virtual key stole selection or physical keyboard focus from the text field");
    // Exercise the shared hold gesture through the Creator's real SDL input,
    // not synthetic LVGL clicks which bypass press tracking and editor handlers.
    lv_textarea_set_text(entry, "");
    auto* heldLabel=Label(lv_advanced_keyboard_get_keyboard(keyboard),"a");
    Require(heldLabel,"Hold gesture key missing");
    lv_obj_update_layout(lv_screen_active());
    lv_area_t heldBounds;lv_obj_get_coords(lv_obj_get_parent(heldLabel),&heldBounds);
    lv_obj_get_transformed_area(lv_obj_get_parent(heldLabel),&heldBounds,LV_OBJ_POINT_TRANSFORM_FLAG_RECURSIVE);
    const float holdX=(heldBounds.x1+heldBounds.x2)/2.0f,holdY=(heldBounds.y1+heldBounds.y2)/2.0f;
    const auto window=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    auto mouse=[&](Uint32 type) {
        SDL_Event event{};event.type=type;
        if(type==SDL_EVENT_MOUSE_MOTION){event.motion.windowID=window;event.motion.x=holdX;event.motion.y=holdY;}
        else {event.button.windowID=window;event.button.button=SDL_BUTTON_LEFT;event.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.x=holdX;event.button.y=holdY;}
        Require(SDL_PushEvent(&event),"Cannot send keyboard hold input");EditorInputTest::Drain(simulator);
    };
    mouse(SDL_EVENT_MOUSE_MOTION);mouse(SDL_EVENT_MOUSE_BUTTON_DOWN);
    const auto holdDeadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1100);
    while(std::chrono::steady_clock::now()<holdDeadline)Require(simulator.ProcessEvents(),"SDL stopped during key hold");
    auto* variant=Label(lv_layer_top(),u8"à");Require(variant,"Holding a Creator simulator key did not open Unicode variants");
    mouse(SDL_EVENT_MOUSE_BUTTON_UP);
    Require(std::string(lv_textarea_get_text(entry)).empty(),"Held source key inserted alongside its variant");
    EditorInputTest::Click(lv_obj_get_parent(variant),simulator);
    Require(std::string(lv_textarea_get_text(entry))==u8"à" && !Label(lv_layer_top(),u8"à"),"Variant was not inserted or its popup remained open");
    Require(properties()->GetSelectedObject()==entry,"Variant selection stole the target field selection");
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
        EditorInputTest::Drain(simulator);
    };
    Require(lv_dropdown_get_option_count(Field(properties(),"Language"))==14 && std::string(lv_advanced_keyboard_get_language(keyboard))=="en-US","Language defaults/list are incorrect");
    selectLanguage("es-ES");
    Require(std::string(lv_advanced_keyboard_get_language(keyboard))=="es-ES" && lv_advanced_keyboard_get_predictor(keyboard),"Spanish selection did not attach its bundled corpus model");
    const auto spanishDefault=editor.ProjectDocument();
    Require(spanishDefault["obj"][1]["advancedKeyboard"]["language"]=="es-ES" && spanishDefault["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==16 &&
        spanishDefault["obj"][1]["advancedKeyboard"]["trieId"]!=englishDefault["obj"][1]["advancedKeyboard"]["trieId"],"Language/model selection was not persisted or refreshed");
    Require(editor.Undo(),"Keyboard language change was not undoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(std::string(lv_advanced_keyboard_get_language(keyboard))=="en-US" && lv_advanced_keyboard_get_predictor(keyboard),"Undo lost the English language/model");
    Require(editor.Redo(),"Keyboard language change was not redoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(std::string(lv_advanced_keyboard_get_language(keyboard))=="es-ES" && lv_advanced_keyboard_get_predictor(keyboard) &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==16,"Redo lost the selected language/model");
    properties()->SetSelectedObject(keyboard);selectLanguage("fr-FR");
    auto selectSize=[&](uint32_t sizeKiB) {
        auto* control=Field(properties(),"Trie size");Require(lv_obj_has_class(control,&lv_dropdown_class),"Trie size is not a dropdown");
        const auto prefix=std::to_string(sizeKiB)+" KiB";std::istringstream options(lv_dropdown_get_options(control));
        std::string option;uint32_t index=0;bool found=false;
        while(std::getline(options,option)){if(option.rfind(prefix,0)==0){found=true;break;}++index;}
        Require(found,"Expected bundled trie size is unavailable");
        lv_dropdown_set_selected(control,index);lv_obj_send_event(control,LV_EVENT_VALUE_CHANGED,nullptr);EditorInputTest::Drain(simulator);
    };
    const auto frenchSmall=editor.ProjectDocument();const auto frenchSmallId=frenchSmall["obj"][1]["advancedKeyboard"]["trieId"];
    selectSize(256);const auto frenchLarge=editor.ProjectDocument();const auto frenchLargeId=frenchLarge["obj"][1]["advancedKeyboard"]["trieId"];
    Require(frenchLargeId!=frenchSmallId && frenchLarge["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==256 &&
        lv_advanced_keyboard_get_predictor(keyboard),"Larger French trie did not replace the small model");
    const auto bundledBinary=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",frenchLarge);
    const auto bundledDecoded=Serialization::Protobuf::Decode(bundledBinary).data;
    Require(bundledDecoded["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==256 && bundledDecoded["obj"][1]["advancedKeyboard"]["trieId"]==frenchLargeId,
        "Protobuf lost bundled model size/identity");
    bool embedded=false;for(const auto& model:bundledDecoded.at("tries"))if(model.at("id")==frenchLargeId)embedded=model.contains("data")&&!model.at("data").empty();
    Require(embedded && bundledDecoded.dump().find("D:\\\\Corpus")==std::string::npos,"Selected corpus model depends on its source drive instead of embedded bytes");
    const auto bundledCount=frenchLarge.at("tries").size();selectSize(16);selectSize(256);
    Require(editor.ProjectDocument()["tries"].size()==bundledCount,"Switching bundled sizes duplicated model assets");
    Require(editor.Undo(),"Trie size change was not undoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==16 &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]==frenchSmallId,"Undo did not restore the small model and its size together");
    Require(editor.Redo(),"Trie size change was not redoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==256 &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]==frenchLargeId,"Redo did not restore the larger model and size together");
    properties()->SetSelectedObject(keyboard);Edit(Field(properties(),"Autocorrect Threshold"),"0.7");
    const uint8_t trie[]={'N','G','T','R',3,0,0,0,4,0,0,0,0,0,0,0,0,2,0,2,'a',1,1,'a',1,1};
    auto path=std::filesystem::temp_directory_path()/("creator-trie-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ngtr");
    {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(trie),sizeof trie);}
    const auto beforeCustomCount=editor.ProjectDocument()["tries"].size();
    try {AdvancedProperties::LoadTrie(properties(),path);AdvancedProperties::LoadTrie(properties(),path);}
    catch(...){std::filesystem::remove(path);throw;}std::filesystem::remove(path);
    const auto imported=editor.ProjectDocument();Require(imported["tries"].size()==beforeCustomCount+1,"Trie import did not content-deduplicate");
    const auto& settings=imported["obj"][1]["advancedKeyboard"];
    const auto customId=settings.at("trieId");
    Require(settings["autocorrectThreshold"]==0.7 && settings["trieSizeKiB"]==0,"Selected custom trie/settings were not saved");
    selectLanguage("de-DE");
    Require(std::string(lv_advanced_keyboard_get_language(keyboard))=="de-DE" && lv_advanced_keyboard_get_predictor(keyboard) &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]==customId && editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==0,
        "Language change discarded an explicitly loaded trie or marked it bundled");
    Require(!settings.contains("text") && !settings.contains("password"),"Keyboard retained private text field properties");
    auto selectTrie=[&](uint32_t index) {
        auto* control=Field(properties(),"Prediction trie");Require(lv_obj_has_class(control,&lv_dropdown_class),"Trie selector is not a dropdown");
        lv_dropdown_set_selected(control,index);lv_obj_send_event(control,LV_EVENT_VALUE_CHANGED,nullptr);
        EditorInputTest::Drain(simulator);
    };
    selectTrie(1);Require(lv_advanced_keyboard_get_predictor(keyboard),"Bundled trie selection did not attach a predictor");
    selectTrie(0);Require(!lv_advanced_keyboard_get_predictor(keyboard),"None trie selection kept its predictor");
    selectLanguage("bn-BD");
    Require(!lv_advanced_keyboard_get_predictor(keyboard) && editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]=="" &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==0 && lv_dropdown_get_option_count(Field(properties(),"Trie size"))==1,
        "A language without bundled models retained another language's trie or offered unavailable sizes");
    uint32_t customChoice=2;for(const auto& model:properties()->Tries().ToJSON()){if(model.at("id")==customId)break;++customChoice;}
    selectTrie(customChoice);Require(lv_advanced_keyboard_get_predictor(keyboard),"Cannot restore the custom project model");
    selectLanguage("zh-CN");
    Require(editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]==customId &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==0,
        "Selecting Mandarin replaced an explicitly selected custom trie with its new bundle");
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
    properties()->SetSelectedObject(keyboard);
    for(const auto* id:{"zh-CN","ar-SA","hi-IN","bn-BD","id-ID","ur-PK","ru-RU","el-GR"}) {
        const auto* authoredFont=lv_obj_get_style_text_font(entry,LV_PART_MAIN);selectLanguage(id);
        const bool rtl=std::string(id)=="ar-SA"||std::string(id)=="ur-PK";
        Require(std::string(lv_advanced_keyboard_get_language(keyboard))==id && lv_advanced_keyboard_get_right_to_left(keyboard)==rtl,"Inspector did not select the script layout/direction");
        Require(lv_obj_get_style_text_font(entry,LV_PART_MAIN)==authoredFont,"Language inspector replaced an authored text font");
        auto* direction=Checkbox(properties()->ObjectPropWin()->GetWindow(),"Right-to-left display");
        Require(direction&&lv_obj_has_state(direction,LV_STATE_CHECKED)==rtl,"RTL property checkbox did not follow the language");
        const auto saved=editor.ProjectDocument();const auto& settings=saved["obj"][1]["advancedKeyboard"];
        Require(settings["language"]==id&&settings["rightToLeft"]==rtl,"Editor JSON lost script language/direction");
        const auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",saved);
        Require(Serialization::Protobuf::Decode(bytes).data["obj"][1]["advancedKeyboard"]["rightToLeft"]==rtl,"Editor Protobuf lost RTL setting");
    }
    selectLanguage("ar-SA");auto* direction=Checkbox(properties()->ObjectPropWin()->GetWindow(),"Right-to-left display");
    lv_obj_remove_state(direction,LV_STATE_CHECKED);lv_obj_send_event(direction,LV_EVENT_VALUE_CHANGED,nullptr);
    Require(!lv_advanced_keyboard_get_right_to_left(keyboard)&&!editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["rightToLeft"].get<bool>(),"RTL property override was not applied");
    Require(editor.Undo(),"RTL edit was not undoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(lv_advanced_keyboard_get_right_to_left(keyboard),"Undo did not restore default Arabic direction");
    Require(editor.Redo(),"RTL edit was not redoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(!lv_advanced_keyboard_get_right_to_left(keyboard),"Redo did not restore the RTL override");
    properties()->SetSelectedObject(keyboard);selectSize(16);
    Require(!lv_advanced_keyboard_get_right_to_left(keyboard) && editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==16,
        "Choosing a bundled size reset the target language's explicit RTL override");
    selectLanguage("zh-CN");
    const auto mandarinSmall=editor.ProjectDocument();const auto mandarinSmallId=mandarinSmall["obj"][1]["advancedKeyboard"]["trieId"];
    const auto* mandarinSmallModel=BundledTries::Find("zh-CN",16);Require(mandarinSmallModel,"Small Mandarin model catalog entry missing");
    Require(lv_advanced_keyboard_get_predictor(keyboard) && !lv_advanced_keyboard_get_right_to_left(keyboard) &&
        mandarinSmallId==mandarinSmallModel->projectTrieId && mandarinSmall["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==16,
        "Switching from Arabic did not select the small Mandarin model and LTR direction");
    selectSize(256);const auto mandarinLarge=editor.ProjectDocument();const auto mandarinLargeId=mandarinLarge["obj"][1]["advancedKeyboard"]["trieId"];
    Require(mandarinLargeId!=mandarinSmallId && mandarinLarge["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==256 &&
        lv_advanced_keyboard_get_predictor(keyboard),"Mandarin size selection did not replace its model");
    const auto mandarinDecoded=Serialization::Protobuf::Decode(Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",mandarinLarge)).data;
    const auto* mandarinModel=BundledTries::Find("zh-CN",256);Require(mandarinModel,"Mandarin model catalog entry missing");
    const auto mandarinBytes=BundledTries::Read(*mandarinModel);bool exactMandarin=false;
    for(const auto& model:mandarinDecoded.at("tries"))if(model.at("id")==mandarinLargeId) {
        Require(model.at("data").is_binary() && model.at("indexData").is_binary() && !model.at("indexData").empty(),
            "Mandarin project did not embed its model and generated index as binary");
        const auto& stored=model.at("data").get_binary();
        exactMandarin=stored.size()==mandarinBytes.size() && std::equal(stored.begin(),stored.end(),mandarinBytes.begin());
    }
    Require(exactMandarin && mandarinDecoded["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==256 &&
        mandarinDecoded.dump().find("D:\\\\Corpus")==std::string::npos,"Saved Mandarin model lost its exact vocabulary/size or retained a corpus drive dependency");
    Require(editor.Undo(),"Mandarin size change was not undoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]==mandarinSmallId &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==16 && lv_advanced_keyboard_get_predictor(keyboard),
        "Undo did not restore the small Mandarin model and its predictor");
    Require(editor.Redo(),"Mandarin size change was not redoable");keyboard=lv_obj_get_child(surface,1);entry=lv_obj_get_child(surface,0);
    Require(editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]==mandarinLargeId &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieSizeKiB"]==256 && lv_advanced_keyboard_get_predictor(keyboard),
        "Redo did not restore the large Mandarin model and its predictor");
    // Both supplied models contain the real 世→界 bigram. A second prior Han
    // character exercises backoff when those models have no retained trigrams.
    for(const auto sizeKiB:{16u,256u}) {
        properties()->SetSelectedObject(keyboard);selectSize(sizeKiB);properties()->SetSelectedObject(entry);
        lv_textarea_set_text(entry,u8"全世");lv_textarea_set_cursor_pos(entry,LV_TEXTAREA_CURSOR_LAST);
        auto* bar=lv_obj_get_child(keyboard,0);lv_obj_t* next=nullptr;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!(next=Label(bar,u8"界")) && std::chrono::steady_clock::now()<deadline)
            Require(simulator.ProcessEvents(),"SDL stopped while waiting for Mandarin trie prediction");
        Require(next && !lv_obj_has_state(lv_obj_get_parent(next),LV_STATE_DISABLED),"Bundled Mandarin model did not predict the next Han character");
        lv_obj_send_event(lv_obj_get_parent(next),LV_EVENT_CLICKED,nullptr);
        Require(std::string(lv_textarea_get_text(entry))==u8"全世界","Accepting a Mandarin trie prediction added spaces or replaced its Han context");
    }
    properties()->SetSelectedObject(entry);
    lv_textarea_set_text(entry,"");EditorInputTest::Text("ni",simulator);
    Require(std::string(lv_textarea_get_text(entry)).empty(),"Physical SDL Pinyin went directly into the target as Latin text");
    auto* candidate=Label(lv_obj_get_child(keyboard,0),u8"你");Require(candidate,"Physical Pinyin input did not show Chinese candidates");
    lv_obj_send_event(lv_obj_get_parent(candidate),LV_EVENT_CLICKED,nullptr);
    Require(std::string(lv_textarea_get_text(entry))==u8"你" && lv_advanced_keyboard_get_predictor(keyboard) &&
        editor.ProjectDocument()["obj"][1]["advancedKeyboard"]["trieId"]==mandarinLargeId,
        "Pinyin input did not coexist with the selected Mandarin trie or inserted Latin spaces");
    std::cout<<"Keyboard language/bundled size selection, exact Mandarin model embedding, Pinyin, custom model preservation, deduplication, undo/redo and C export passed\n";
}
