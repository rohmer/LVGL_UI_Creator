#include "DesignTools.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/ProjectThemes/ThemeGallery.h"
#include "../../../UICreator/Components/ComponentLibrary.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include <Serialization/ProjectDesign.h>
#include <Serialization/Protobuf.h>
#include <iostream>

namespace {
namespace D=Serialization::ProjectDesign;
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
lv_obj_t* Label(lv_obj_t* root,const char* caption)
{
    if(lv_obj_has_class(root,&lv_label_class)&&std::string(lv_label_get_text(root))==caption)return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=Label(lv_obj_get_child(root,i),caption))return found;return nullptr;
}
lv_obj_t* First(lv_obj_t* root,const lv_obj_class_t* type)
{
    if(lv_obj_has_class(root,type))return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=First(lv_obj_get_child(root,i),type))return found;return nullptr;
}
lv_obj_t* Window(const char* title)
{auto* label=Label(lv_layer_top(),title);Require(label,"Authoring window is missing");return lv_obj_get_parent(lv_obj_get_parent(label));}
void Click(lv_obj_t* root,const char* text)
{auto* label=Label(root,text);Require(label,"Design tool action is missing");lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);}
json Record(int oid,int parent,int x,int y,int width,int height)
{return {{"oid",oid},{"par",parent},{"base",{{"coords",{{"x",x},{"y",y},{"width",width},{"height",height}}}}}};}
}
void CheckDesignToolsEditor(UI& editor,SdlSimulator& simulator)
{
    CheckProjectDesignCore();
    auto document=editor.ProjectDocument();document["obj"]=json::array({Record(0,-1,10,10,280,180),Record(1,0,8,8,180,28),Record(2,0,8,55,220,42)});
    document["obj"][1]["text"]="Account details";document["obj"][2]["textarea"]={{"text","Your name"}};editor.ApplyDocument(document);editor.SelectObjectIds({0});
    editor.MenuAction("theme-gallery");auto* gallery=Window("Theme gallery");
    for(const auto& p:D::Presets())Require(Label(gallery,p.at("name").get_ref<const std::string&>().c_str()),"Theme gallery omitted a preset");
    Click(gallery,"Deep Ocean");Click(gallery,"Apply to project");
    document=editor.ProjectDocument();Require(D::ThemeId(document)=="ocean-dark","Theme gallery did not apply the chosen palette");
    Require((D::Style(document["obj"][0])["bg_color"].get<unsigned>()&0xffffffu)==D::Preset("ocean-dark")["tokens"]["surface"],"Applied theme is missing from serialized native styles");
    auto* role=First(gallery,&lv_dropdown_class);auto* value=First(gallery,&lv_textarea_class);Require(role&&value,"Theme token editor is missing");
    lv_dropdown_set_selected(role,4);lv_obj_send_event(role,LV_EVENT_VALUE_CHANGED,nullptr);lv_textarea_set_text(value,"#326699");Click(gallery,"Update shared token");
    Require(D::Tokens(editor.ProjectDocument(),"ocean-dark")["primary"]==0x326699,"Shared theme token was not saved");
    auto roundtrip=editor.ProjectDocument();D::SetToken(roundtrip,"ocean-dark","surface",0x183746);editor.ApplyDocument(roundtrip);
    roundtrip=editor.ProjectDocument();Require((D::Style(roundtrip["obj"][0])["bg_color"].get<unsigned>()&0xffffffu)==0x183746,"Shared token stopped updating after a native LVGL save/load roundtrip");
    Require(roundtrip["obj"][0]["base"]["design"]["theme"]["bindings"].contains("bg_color"),"Native ARGB serialization was mistaken for a local override");
    Require(editor.Undo(),"Roundtrip surface edit is not undoable");
    ProjectThemeWindow::Close();Require(editor.Undo(),"Theme token update is not undoable");Require(D::Tokens(editor.ProjectDocument(),"ocean-dark")["primary"]==D::Preset("ocean-dark")["tokens"]["primary"],"Undo did not restore shared token");
    Require(editor.Redo(),"Theme token redo failed");editor.SelectObjectIds({0});editor.MenuAction("component-library");auto* library=Window("Component library");
    auto* name=First(library,&lv_textarea_class);Require(name,"Component name field is missing");lv_textarea_set_text(name,"Account card");Click(library,"Capture selection");
    document=editor.ProjectDocument();Require(document["components"].size()==1&&document["components"][0]["records"].size()==3,"Component capture lost selected descendants");
    Click(library,"Insert linked instance");document=editor.ProjectDocument();Require(document["obj"].size()==6,"Component insertion lost widgets");
    Require(D::InstanceCount(document,document["components"][0]["id"].get<std::string>())==1,"Inserted component is not linked");
    document["obj"][4]["text"]="Updated master card";editor.ApplyDocument(document);editor.SelectObjectIds({3});Click(library,"Update selected master");Click(library,"Apply master update");
    document=editor.ProjectDocument();Require(document["components"][0]["revision"]==2,"Component master revision did not advance");
    Require(document["components"][0]["records"][1]["text"]=="Updated master card","Edited source did not update master definition");
    const auto protobuf=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",document);
    const auto decoded=Serialization::Protobuf::Decode(protobuf).data;Require(decoded["components"]==document["components"]&&decoded["projectTheme"]==document["projectTheme"],"Project protobuf lost component/theme authoring data");
    editor.SelectObjectIds({3});Click(library,"Detach selected instance");document=editor.ProjectDocument();Require(D::InstanceCount(document,document["components"][0]["id"].get<std::string>())==0,"Component detach failed");
    ComponentLibraryWindow::Close();Require(editor.Undo(),"Component detach is not undoable");
    document=editor.ProjectDocument();Require(D::InstanceCount(document,document["components"][0]["id"].get<std::string>())==1,"Undo failed to restore component link");
    for(int i=0;i<3;++i)simulator.ProcessEvents();
    editor.MenuAction("theme-gallery");
    std::cout<<"Native theme gallery, token editor, undo/redo, linked component library, master updates, protobuf persistence and detach passed\n";
}
