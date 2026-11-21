#include "WorkflowProblems.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Validation/ProblemsPanel.h"
#include "../../../UICreator/Validation/AccessibilityChecks.h"
#include "../../../UICreator/Devices/DeviceWindow.h"
#include "../../../UICreator/Devices/DeviceTargets.h"
#include <Serialization/FocusOrder.h>
#include <Serialization/LVObject.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
lv_obj_t* Label(lv_obj_t* root,const char* text){if(lv_obj_has_class(root,&lv_label_class)&&std::string(lv_label_get_text(root))==text)return root;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* result=Label(lv_obj_get_child(root,i),text))return result;return nullptr;}
bool Text(lv_obj_t* root,const char* text){if(lv_obj_has_class(root,&lv_label_class)&&std::string(lv_label_get_text(root)).find(text)!=std::string::npos)return true;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(Text(lv_obj_get_child(root,i),text))return true;return false;}
lv_obj_t* Field(lv_obj_t* root,const char* text){auto* label=Label(root,text);Require(label,"Workflow field is missing");return lv_obj_get_child(lv_obj_get_parent(label),1);}
void Click(lv_obj_t* root,const char* text){auto* label=Label(root,text);Require(label,"Workflow action is missing");auto* button=lv_obj_get_parent(label);Require(lv_obj_has_class(button,&lv_button_class),"Workflow action parent is not a button");lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);}
void Pump(SdlSimulator& simulator,unsigned duration){const auto end=SDL_GetTicks()+duration;while(SDL_GetTicks()<end)simulator.ProcessEvents();}
json Record(int oid,const char* type,int x,int y,int width,int height){return {{"oid",oid},{"par",-1},{"base",{{"name",std::string(type)+std::to_string(oid)},{"coords",{{"x",x},{"y",y},{"width",width},{"height",height}}}}},{type,json::object()}};}
}
void CheckWorkflowProblems(UI& editor,SdlSimulator& simulator){
    auto small=Record(0,"button",10,10,20,20);small["base"]["accessibility"]["tabIndex"]=0;
    auto button=Record(1,"button",80,10,80,48);button["base"]["accessibility"]["tabIndex"]=0;
    auto text=Record(2,"text",10,90,90,25);text["text"]="Caption";text["base"]["translationKey"]="caption";text["base"]["nativeStyles"]=json::array({{{"selector",0},{"values",{{"text_color",0xff777777u},{"bg_color",0xff777777u},{"bg_opa",255}}}}});
    json document={{"formatVersion",2},{"canvas",{{"width",320},{"height",240}}},{"obj",json::array({small,button,text})},{"variables",json::array()},
        {"translations",{{"defaultLocale","en"},{"locales",{{"en",{{"caption","Caption"}}},{"fr",{{"caption","This deliberately long translated label cannot fit inside the small text area"}}}}}}}};
    editor.ApplyDocument(document);editor.SelectObjectIds({0});auto* simulatorTitle=Label(lv_screen_active(),"Simulator [Size:320x240]");Require(simulatorTitle,"Authored simulator is missing");auto* surface=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(simulatorTitle))),0);auto* properties=static_cast<ObjectUserData*>(lv_obj_get_user_data(lv_obj_get_child(surface,0)))->toolTray->GetPropertyWindow();
    uint64_t revision=1;ProblemsPanel::Context context;context.document=[&]{return editor.ProjectDocument();};context.apply=[&](json value){editor.ApplyDocument(std::move(value));++revision;};context.applyMetadata=[&](json value){auto updated=editor.ProjectDocument();for(const auto& entry:value.items())updated[entry.key()]=entry.value();editor.ApplyDocument(std::move(updated));++revision;};context.revision=[&]{return revision;};context.selectObject=[&](size_t,int oid){editor.SelectObjectIds({oid});};
    auto* panel=ProblemsPanel::Create(lv_layer_top(),properties,context);lv_obj_set_size(panel,900,650);lv_obj_center(panel);Require(ProblemsPanel::IssueCount(panel)>=4,"Problems omitted touch/contrast/focus/translated-text findings");Require(Text(panel,"Contrast")&&Text(panel,"Focus order")&&Text(panel,"Translations"),"Accessibility categories were not displayed");const auto initial=ProblemsPanel::AnalysisCount(panel);Pump(simulator,400);Require(ProblemsPanel::AnalysisCount(panel)==initial,"Idle Problems panel repeatedly serialized or analyzed the project");
    Click(panel,"Make touch target at least 44 px");Require(editor.ProjectDocument()["obj"][0]["base"]["coords"]["width"].get<int>()>=44,"Touch quick fix did not update the authored widget");Require(editor.Undo(),"Problems quick fix was not undoable");++revision;Pump(simulator,400);Require(editor.ProjectDocument()["obj"][0]["base"]["coords"]["width"]==20,"Undo did not restore touch dimensions");
    editor.SelectObjectIds({0});Pump(simulator,120);lv_textarea_set_text(Field(panel,"Focus index"),"2");Click(panel,"Set focus index");Require(editor.ProjectDocument()["obj"][0]["base"]["accessibility"]["tabIndex"]==2,"Focus order editor did not persist the selected index");
    auto changed=editor.ProjectDocument();changed["variables"]=json::array({{{"var",{{"name","missing"},{"dataType",1},{"default",0},{"oid",999},{"property","value"}}}}});Require(!ScreenValidation::BlockingErrors(changed).empty(),"Broken widget bindings did not block export/build validation");
    {
        auto* previous=lv_group_get_default();auto* group=lv_group_create();lv_group_set_default(group);auto* parent=lv_obj_create(lv_screen_active());
        auto* a=Serialization::LVObject::FromJSON({{"accessibility",{{"tabIndex",1}}}},lv_button_create(parent));auto* b=Serialization::LVObject::FromJSON({{"accessibility",{{"tabIndex",-1}}}},lv_button_create(parent));auto* c=Serialization::LVObject::FromJSON({{"accessibility",{{"tabIndex",0}}}},lv_button_create(parent));
        Serialization::Accessibility::ApplyFocusOrder(group,{a,b,c});Require(lv_group_get_obj_count(group)==2&&lv_group_get_obj_by_index(group,0)==c&&lv_group_get_obj_by_index(group,1)==a,"Runtime focus order did not reorder or exclude widgets");lv_obj_delete(parent);lv_group_delete(group);lv_group_set_default(previous);
    }
    const auto work=std::filesystem::absolute("workflow-device-project");std::filesystem::create_directories(work/"ui");{std::ofstream output(work/"CMakeLists.txt");output<<"cmake_minimum_required(VERSION 3.24)\nproject(WorkflowFixture LANGUAGES C)\n";}{std::ofstream output(work/"ui/project.json");output<<changed.dump();}
    DeviceWindow::SetContext({[&]{return editor.ProjectDocument();},context.apply,context.applyMetadata});const auto before=editor.ProjectDocument();DeviceWindow::OpenProject(properties,work,{{"environment",{"CMake"}}});auto* devices=DeviceWindow::ActiveWindow();Require(devices,"Connected build workflow did not open");Require(std::string(lv_textarea_get_text(Field(devices,"Project folder")))==work.u8string(),"Generated project folder was not handed to the build workflow");Require(lv_dropdown_get_selected(Field(devices,"Toolchain"))==2,"Generated CMake toolchain was not selected");Require(editor.ProjectDocument()==before,"Opening a build target changed the project");
    lv_textarea_set_text(Field(devices,"Target name"),"Desktop test");Click(devices,"Save target");devices=DeviceWindow::ActiveWindow();Require(editor.ProjectDocument()["deviceTargets"].size()==1,"Named project target was not saved");Click(devices,"Build");Require(!DeviceWindow::Running()&&Text(devices,"Build blocked by project validation"),"A broken generated project started a compiler process");DeviceWindow::Close();ProblemsPanel::Close(panel);DeviceWindow::SetContext({});
    Require(ScreenValidation::ContrastRatio(0,0xffffff)>20.9&&ScreenValidation::ContrastRatio(0x777777,0x777777)==1.0,"Contrast math is incorrect");
    std::cout<<"Problems debounce/quick fixes, contrast/focus/translation checks, runtime focus order, generated-project handoff, target persistence and build validation passed\n";
}
