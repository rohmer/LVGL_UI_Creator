#include "Interactions.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Interactions/InteractionEditor.h"
#include "../../../UICreator/Validation/ScreenValidation.h"
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Protobuf.h>
#include <algorithm>
#include <stdexcept>
#include <string>
namespace {
using json=nlohmann::json;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
lv_obj_t* Label(lv_obj_t* root,const char* text){if(lv_obj_has_class(root,&lv_label_class)&&std::string(lv_label_get_text(root))==text)return root;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* child=Label(lv_obj_get_child(root,i),text))return child;return nullptr;}
void Click(const char* text){auto* label=Label(lv_layer_top(),text);Require(label,"Expected action button missing");auto* button=lv_obj_get_parent(label);Require(lv_obj_has_class(button,&lv_button_class),"Action button parent mismatch");lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);}
lv_obj_t* Field(const char* title){auto* label=Label(lv_layer_top(),title);Require(label,"Expected editor field missing");return lv_obj_get_child(lv_obj_get_parent(label),lv_obj_get_index(label)+1);}
json Widget(int oid,const char* kind,int x,int y,int width,int height){return {{"oid",oid},{"par",-1},{"base",{{"name",std::string(kind)+std::to_string(oid)},{"coords",{{"x",x},{"y",y},{"width",width},{"height",height}}}}},{kind,json::object()}};}
}
void CheckInteractionsEditor(UI& editor,SdlSimulator&) {
    auto button=Widget(0,"button",8,8,90,48),other=Widget(1,"button",55,8,90,48),small=Widget(2,"button",8,90,20,20),container=Widget(3,"container",150,10,120,160),child=Widget(4,"button",5,5,80,48);child["par"]=3;
    auto broken=Widget(5,"button",150,190,90,48);broken["base"]["events"]=json::array({{{"event","clicked"},{"action","hide"},{"targetOid",999}}});broken["base"]["fontId"]="missing-font";
    json document={{"canvas",{{"width",320},{"height",240}}},{"obj",json::array({button,other,small,container,child,broken})},{"variables",json::array()}};
    auto issues=ScreenValidation::Analyze(document);auto has=[&](const char* group,int oid){return std::any_of(issues.begin(),issues.end(),[&](const ScreenValidation::Issue& issue){return issue.group==group&&issue.oid==oid;});};Require(has("Overlap",0),"Sibling overlap not detected");Require(has("Touch targets",2),"Small touch target not detected");Require(has("Actions",5),"Broken action target not detected");Require(has("Assets",5),"Missing asset not detected");Require(!has("Overlap",3),"Intentional parent/child overlap reported");
    document["obj"]=json::array({button,other});editor.ApplyDocument(document);editor.SelectObjectIds({0});editor.MenuAction("events-editor");Require(Label(lv_layer_top(),"Events and actions"),"Event editor did not open");Click("Add action");lv_dropdown_set_selected(Field("Action"),4);lv_dropdown_set_selected(Field("Target widget (show / hide / toggle / validate)"),2);Click("Save action");auto saved=editor.ProjectDocument();Require(saved["obj"][0]["base"]["events"][0]["action"]=="hide","Event editor did not save selected action");Require(saved["obj"][0]["base"]["events"][0]["targetOid"]==1,"Event editor saved a tree ID instead of dense object ID");InteractionEditor::Close();
    auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",saved);auto roundtrip=Serialization::Protobuf::Decode(bytes).data;Require(roundtrip["obj"][0]["base"]["events"]==saved["obj"][0]["base"]["events"],"Protobuf lost authored events");
    editor.MenuAction("run-toggle");Require(editor.Running(),"Run preview did not start with interactions");auto* source=lvgl_runtime_object(editor.RunRuntime(),0,0);auto* target=lvgl_runtime_object(editor.RunRuntime(),0,1);Require(source&&target,"Run preview objects missing");lv_obj_send_event(source,LV_EVENT_CLICKED,nullptr);Require(lv_obj_is_hidden(target),"Authored action did not execute in Run mode");editor.MenuAction("run-stop");Require(!editor.ProjectDocument()["obj"][1]["base"].value("hidden",false),"Run action modified authored design");
    editor.MenuAction("validate-screen");Require(Label(lv_layer_top(),"Validate screens"),"Validation report did not open");ScreenValidation::Close();
}
