#include "UI.h"
#include <Serialization/ProjectScreens.h>
#include <Serialization/ProjectDesign.h>
#include "../Utils/EditorSession.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/CommandPalette.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorIcons.h"
#include "../Screens/ScreenFlowWindow.h"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
const char* metadataKeys[]={"projectTheme","components","sharedStyles","previewProfiles","deviceTargets","accessibility","translations","advancedKeyboard"};
}
void UI::InvalidateDocument(){++documentRevision;cachedRevision=0;serializedRevision=0;}
json UI::BuildProjectDocument()const{
    lv_obj_update_layout(simWindow->GetDrawSurface());
    auto doc=Serialization::Project::ToJSON(propertyWindow->GetObjectTree()->GetObjects(),simWindow->GetDrawSurface(),toolBar->VariablesToJSON(),simWindow->GetWidth(),simWindow->GetHeight(),propertyWindow->Fonts().ToJSON(),propertyWindow->Images().ToJSON(),propertyWindow->Tries().ToJSON());
    for(const auto& item:projectMetadata.items())doc[item.key()]=item.value();
    if(!screenRecords.empty()){
        doc["screens"]=screenRecords;doc["editorScreen"]=editingScreen;doc["activeScreen"]=startScreen;
        if(screenContentDirty)Serialization::ProjectScreens::SyncAlias(doc);
        else Serialization::ProjectScreens::Select(doc,editingScreen);
    }
    return doc;
}
const json& UI::CachedDocument()const{
    if(cachedRevision!=documentRevision){serializingDocument=true;try{cachedDocument=BuildProjectDocument();serializingDocument=false;}catch(...){serializingDocument=false;throw;}cachedRevision=documentRevision;++serializationCount;}
    return cachedDocument;
}
const std::string& UI::SerializedDocument()const{
    CachedDocument();if(serializedRevision!=documentRevision){cachedSerialized=Serialization::ProjectScreens::Canonical(cachedDocument).dump();serializedRevision=documentRevision;}
    return cachedSerialized;
}
void UI::ApplyMetadata(json patch){
    if(!patch.is_object())throw std::invalid_argument("Project metadata must be an object");
    for(const auto& item:patch.items())if(std::find(std::begin(metadataKeys),std::end(metadataKeys),item.key())==std::end(metadataKeys))throw std::invalid_argument("Unknown project metadata: "+item.key());
    EditorEdit::Scope edit(this);for(auto& item:patch.items())projectMetadata[item.key()]=std::move(item.value());InvalidateDocument();
}
bool UI::SelectScreen(size_t index){
    if(index>=ScreenCount())return false;if(index==editingScreen){propertyWindow->SetSelectedObject(simWindow->GetDrawSurface());RefreshScreensDock();return true;}
    auto doc=ProjectDocument();Serialization::ProjectScreens::Select(doc,index);
    auto loaded=Serialization::Project::FromJSON(doc);
    switchingScreen=true;const bool previous=restoringHistory;restoringHistory=true;
    try{InstallProject(std::move(loaded));restoringHistory=previous;switchingScreen=false;history.BreakGroup();return true;}
    catch(...){restoringHistory=previous;switchingScreen=false;throw;}
}
void UI::SelectObjectOnScreen(size_t screen,int oid){
    if(!SelectScreen(screen))return;
    const auto doc=ProjectDocument();const auto objects=propertyWindow->GetObjectTree()->GetObjects();
    for(size_t i=0;i<objects.size()&&i<doc["obj"].size();++i)if(doc["obj"][i].value("oid",int(i))==oid){propertyWindow->SetSelectedObject(objects[i]);lv_obj_scroll_to_view_recursive(objects[i],LV_ANIM_OFF);return;}
}
size_t UI::AddScreen(const std::string& name){auto doc=ProjectDocument();auto index=Serialization::ProjectScreens::Add(doc,name.empty()?"Screen "+std::to_string(ScreenCount()+1):name);ApplyDocument(std::move(doc));return index;}
size_t UI::DuplicateScreen(size_t screen){auto doc=ProjectDocument();auto index=Serialization::ProjectScreens::Duplicate(doc,screen);ApplyDocument(std::move(doc));return index;}
void UI::RenameScreen(size_t screen,const std::string& name){auto doc=ProjectDocument();Serialization::ProjectScreens::Rename(doc,screen,name);ApplyDocument(std::move(doc));}
void UI::MoveScreen(size_t screen,size_t target){auto doc=ProjectDocument();Serialization::ProjectScreens::Move(doc,screen,target);ApplyDocument(std::move(doc));}
void UI::DeleteScreen(size_t screen){auto doc=ProjectDocument();Serialization::ProjectScreens::Remove(doc,screen);ApplyDocument(std::move(doc));}
void UI::SetStartScreen(size_t screen){auto doc=ProjectDocument();Serialization::ProjectScreens::SetStart(doc,screen);ApplyDocument(std::move(doc));}
void UI::RefreshScreensDock(){
    ScreenFlowWindow::Refresh();
    if(!screensDock)return;auto* content=EditorWindow::Content(screensDock);lv_obj_clean(content);
    lv_obj_set_flex_flow(content,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(content,5,0);
    auto* project=lv_button_create(content);lv_obj_set_size(project,LV_PCT(100),36);
    lv_obj_set_click_focusable(project,false);lv_obj_set_style_shadow_width(project,0,0);
    lv_obj_set_state(project,LV_STATE_CHECKED,propertyWindow->ShowingProjectProperties());
    lv_obj_set_style_bg_color(project,lv_color_hex(propertyWindow->ShowingProjectProperties()?0x32683e:0x343e49),0);
    auto* projectName=lv_label_create(project);lv_label_set_text(projectName,"Project");EditorIcons::Set(project,"settings",false,18);
    lv_obj_add_event_cb(project,[](lv_event_t* event){
        if(lv_event_get_code(event)==LV_EVENT_CLICKED)static_cast<UI*>(lv_event_get_user_data(event))->MenuAction("project-settings");
    },LV_EVENT_CLICKED,this);
    auto* screens=lv_label_create(content);lv_label_set_text(screens,"Screens");
    for(size_t i=0;i<ScreenCount();++i){
        auto* button=lv_button_create(content);lv_obj_set_size(button,LV_PCT(100),32);lv_obj_set_click_focusable(button,false);lv_obj_set_style_shadow_width(button,0,0);
        const bool selected=i==editingScreen&&!propertyWindow->ShowingProjectProperties();
        lv_obj_set_state(button,LV_STATE_CHECKED,selected);lv_obj_set_style_bg_color(button,lv_color_hex(selected?0x32683e:0x343e49),0);
        const auto name=screenRecords.empty()?std::string("Screen"):screenRecords[i].value("name",std::string("Screen"));auto* text=lv_label_create(button);lv_label_set_text(text,name.c_str());EditorIcons::Set(button,i==startScreen?"screen-home":"select-screen",false,18);
        struct Selection{UI* ui;size_t screen;};
        lv_obj_add_event_cb(button,[](lv_event_t* event){auto* data=static_cast<Selection*>(lv_event_get_user_data(event));if(lv_event_get_code(event)==LV_EVENT_DELETE)delete data;else if(lv_event_get_code(event)==LV_EVENT_CLICKED)data->ui->SelectScreen(data->screen);},LV_EVENT_ALL,new Selection{this,i});
    }
}
void UI::SaveProjectTo(const std::filesystem::path& path){
    if(path.empty())throw std::invalid_argument("Choose a project file");
    auto doc=ProjectDocument();if(Serialization::ProjectScreens::Multiple(doc)){doc.erase("obj");doc.erase("canvas");}
    EditorSession::WriteAtomic(path,doc.dump(2));projectPath=path.u8string();hasProjectPath=true;MarkSaved();
}
void UI::OpenCommandPalette(bool properties){
    std::vector<CommandPalette::Entry> entries;
    if(!properties){
        for(const auto* menu:{"File","Edit","View","Assets","Language Models","Export","Debug","Help","Align","Distribute","Show panel"})for(const auto& item:MenuItems(menu))if(!item.action.empty()&&item.action!="commands"&&item.action!="property-search"){
            const auto action=item.action;entries.push_back({item.label,std::string(menu),item.shortcut,[this,action]{MenuAction(action);},item.enabled,action=="run-toggle"&&Running()?"run-stop":action});
        }
        entries.push_back({"Add screen","Screens","",[this]{AddScreen();},true,"screen-add"});
        entries.push_back({"Project settings","Project","",[this]{MenuAction("project-settings");},true,"settings"});
    }else{
        struct Target{
            lv_obj_t* object;explicit Target(lv_obj_t* o):object(o){lv_obj_add_event_cb(o,Deleted,LV_EVENT_DELETE,this);}
            static void Deleted(lv_event_t* event){static_cast<Target*>(lv_event_get_user_data(event))->object=nullptr;}
            ~Target(){if(object)lv_obj_remove_event_cb_with_user_data(object,Deleted,this);}
        };
        auto* manager=propertyWindow->GetCWM();
        std::function<void(lv_obj_t*)> walk=[&](lv_obj_t* object){
            const bool control=lv_obj_has_class(object,&lv_textarea_class)||lv_obj_has_class(object,&lv_dropdown_class)||lv_obj_has_class(object,&lv_checkbox_class);
            if(control){
                auto group=manager->GroupFor(object);std::string caption;
                if(lv_obj_has_class(object,&lv_checkbox_class))caption=lv_checkbox_get_text(object);
                else for(auto* parent=lv_obj_get_parent(object);parent&&caption.empty()&&parent!=propertyWindow->GetWindow();parent=lv_obj_get_parent(parent)){
                    for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i){auto* label=lv_obj_get_child(parent,i);if(lv_obj_has_class(label,&lv_label_class)){caption=lv_label_get_text(label);if(!caption.empty())break;}}
                }
                if(!group.empty()&&!caption.empty()){
                    auto target=std::make_shared<Target>(object);
                    entries.push_back({caption,group,"",[this,manager,target]{if(target->object){EditorWindow::Show(lv_obj_get_parent(propertyWindow->GetWindow()));manager->Reveal(target->object);FocusEditorControl(propertyWindow->GetKBGroup(),target->object);}} ,!lv_obj_is_disabled(object),"property-search"});
                }
            }
            if(!lv_obj_has_class(object,&lv_textarea_class))for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)walk(lv_obj_get_child(object,i));
        };
        walk(propertyWindow->GetWindow());
    }
    CommandPalette::Open(properties?"Find property":"Commands",std::move(entries),propertyWindow->GetKBGroup());
}
