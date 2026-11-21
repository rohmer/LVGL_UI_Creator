#define SDL_MAIN_HANDLED
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include <Serialization/ObjectSerializer.h>
#include <iostream>
#include <stdexcept>

namespace
{
void Require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
lv_obj_t* Text(lv_obj_t* root, const char* text)
{
    if(lv_obj_get_class(root)==&lv_label_class && std::string(lv_label_get_text(root))==text)return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* result=Text(lv_obj_get_child(root,i),text))return result;
    return nullptr;
}
lv_obj_t* Checkbox(lv_obj_t* root, const char* text)
{
    if(lv_obj_get_class(root)==&lv_checkbox_class && std::string(lv_checkbox_get_text(root))==text)return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* result=Checkbox(lv_obj_get_child(root,i),text))return result;
    return nullptr;
}
void Click(const char* text)
{
    auto* label=Text(lv_screen_active(),text);Require(label,"Missing editor action");
    lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);
}
}
int main()
{
    try {
        SdlSimulator simulator;Require(simulator.Initialize(1440,960),"SDL initialization failed");
        UI editor(simulator.GetKeyboard());lv_obj_update_layout(lv_screen_active());
        Click("Button");
        lv_obj_t* widget=nullptr;PropertyWindow* properties=nullptr;
        // Simulator uses editor chrome, so follow its title to the content host.
        auto* title=Text(lv_screen_active(),"Simulator [Size:320x240]");Require(title,"Simulator missing");
        auto* simulatorWindow=lv_obj_get_parent(lv_obj_get_parent(title));
        auto* host=EditorWindow::Content(simulatorWindow);
        Require(lv_obj_get_child_count(host)>0,"Simulator screen missing");
        auto* screen=lv_obj_get_child(host,0);Require(lv_obj_get_child_count(screen)>0,"Created widget missing");
        widget=lv_obj_get_child(screen,0);
        auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(widget));Require(data && data->toolTray,"Widget editor metadata missing");
        properties=data->toolTray->GetPropertyWindow();properties->SetSelectedObject(widget);
        Require(properties->RenameSelected("  Status button  "),"Rename failed");
        Require(Serialization::ObjectSerializer::SerializeObject(widget)["base"]["name"]=="Status button","Rename was not serialized");
        Require(!properties->RenameSelected(" \t"),"Empty name accepted");
        auto* xLabel=Text(properties->GetWindow(),"X");Require(xLabel,"X field missing");
        auto* x=lv_obj_get_child(lv_obj_get_parent(xLabel),1);
        lv_textarea_set_text(x,"41");lv_obj_send_event(x,LV_EVENT_VALUE_CHANGED,nullptr);
        Require(lv_obj_get_x(widget)==41,"Valid coordinate was not applied");
        lv_textarea_set_text(x,"-");lv_obj_send_event(x,LV_EVENT_VALUE_CHANGED,nullptr);
        Require(lv_obj_get_x(widget)==41,"Incomplete number changed project geometry");
        Click("Reset position");Require(lv_obj_get_x(widget)==0 && lv_obj_get_y(widget)==0,"Position reset failed");
        auto* lock=Checkbox(properties->GetWindow(),"Lock position");Require(lock,"Position lock missing");
        lv_obj_add_state(lock,LV_STATE_CHECKED);lv_obj_send_event(lock,LV_EVENT_VALUE_CHANGED,nullptr);
        auto saved=Serialization::ObjectSerializer::SerializeObject(widget);
        Require(saved["base"]["drag"]==false,"Position lock was not serialized");
        Require(lv_obj_has_state(x,LV_STATE_DISABLED),"Locked position fields remain editable");
        auto* hidden=Checkbox(properties->GetWindow(),"Hidden on screen");Require(hidden,"Visibility control missing");
        lv_obj_add_state(hidden,LV_STATE_CHECKED);lv_obj_send_event(hidden,LV_EVENT_CLICKED,nullptr);
        Require(lv_obj_is_hidden(widget),"Visibility control failed");
        saved=Serialization::ObjectSerializer::SerializeObject(widget);
        auto* restored=Serialization::ObjectSerializer::DeserializeObject(saved);
        Require(restored && lv_obj_is_hidden(restored),"Visibility did not round trip");
        auto restoredRecord=Serialization::ObjectSerializer::SerializeObject(restored);
        Require(restoredRecord["base"]["drag"]==false && restoredRecord["base"]["name"]=="Status button","Editor metadata did not round trip");
        lv_obj_delete(restored);
        properties->GetObjectTree()->BeginRename(widget);
        auto findEntry=[&](auto&& self,lv_obj_t* object)->lv_obj_t* {
            if(lv_obj_get_class(object)==&lv_textarea_class && std::string(lv_textarea_get_text(object))=="Status button")return object;
            for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)if(auto* found=self(self,lv_obj_get_child(object,i)))return found;
            return nullptr;
        };
        auto* rename=findEntry(findEntry,properties->GetObjectTree()->GetTreeViewUI());Require(rename,"Inline hierarchy editor missing");
        lv_textarea_set_text(rename,"Renamed inline");lv_obj_send_event(rename,LV_EVENT_READY,nullptr);
        Require(Serialization::ObjectSerializer::SerializeObject(widget)["base"]["name"]=="Renamed inline","Inline name was not saved");
        for(int i=0;i<4;++i)simulator.ProcessEvents();
        Require(editor.Undo(),"Inline rename has no undo step");
        Require(editor.ProjectDocument().dump().find("Renamed inline")==std::string::npos,"Undo did not restore the prior name");
        Require(editor.Redo(),"Inline rename has no redo step");
        Require(editor.ProjectDocument().dump().find("Renamed inline")!=std::string::npos,"Redo did not restore the new name");
        std::cout<<"Inspector validation, reset, naming, locking and visibility passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
