#include "Variables.h"
#include "EditorInputTest.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Windows/ToolBar.h"
#include "../../../UICreator/Utils/EditorIcons.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Validation/AccessibilityChecks.h"
#include <Serialization/LVVariable.h>
#include <Serialization/ProjectScreens.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace Input=EditorInputTest;
using json=nlohmann::json;
struct ClipboardRestore {
    std::string previous;
    ClipboardRestore(){char* value=SDL_GetClipboardText();if(value){previous=value;SDL_free(value);}}
    ~ClipboardRestore(){SDL_SetClipboardText(previous.c_str());}
};
void Capture(SdlSimulator& simulator,const std::filesystem::path& path);
json Widget(int oid,const char* name,int y){return {{"oid",oid},{"par",-1},{"base",{{"name",name},{"drag",true},{"coords",{{"x",16},{"y",y},{"width",220},{"height",48}}}}}};}
json Fixture(){
    auto slider=Widget(20,"Main slider",16);slider["slider"]={{"value",0}};
    auto label=Widget(30,"Main readout",80);label["text"]="Original";
    auto button=Widget(40,"Main action",144);button["button"]={{"text","Increment"}};
    button["base"]["events"]=json::array({{{"event","clicked"},{"action","increment_variable"},{"variable","seed"},{"value",1},{"condition",{{"variable","seed"},{"operator","less"},{"value",50}}}},{{"event","clicked"},{"action","navigate"},{"screen",1}}});
    auto other=Widget(60,"Other readout",16);other["text"]="Other";
    auto setter=Widget(70,"Other action",80);setter["button"]={{"text","Set"}};
    setter["base"]["events"]=json::array({{{"event","clicked"},{"action","set_variable"},{"variable","seed"},{"value",11},{"condition",{{"variable","seed"},{"operator","less"},{"value",50}}}}});
    return {{"formatVersion",2},{"activeScreen",0},{"editorScreen",0},
        {"screens",json::array({{{"id","main"},{"name","Main"},{"canvas",{{"width",480},{"height",320}}},{"obj",json::array({slider,label,button})}},{{"id","other"},{"name","Other"},{"canvas",{{"width",480},{"height",320}}},{"obj",json::array({other,setter})}}})},
        {"variables",json::array({{{"var",{{"name","seed"},{"dataType",1},{"default",3},{"history",4},{"bindings",json::array()}}}}})}};
}
json VarRecord(const json& document,const std::string& name="counter"){
    for(const auto& item:document.at("variables"))if(item.at("var").at("name")==name)return item.at("var");
    throw std::runtime_error("Expected authored variable is missing: "+name);
}
int WidgetId(const json& document,size_t screen,const char* name){
    for(const auto& object:document.at("screens").at(screen).at("obj"))if(object.at("base").at("name")==name)return object.at("oid");
    throw std::runtime_error(std::string("Expected authored widget is missing: ")+name);
}
std::string TargetName(const json& document,const json& binding){
    const auto screen=binding.value("screen",size_t(0));const int oid=binding.at("oid");
    Input::Require(screen<document.at("screens").size(),"Variable connection references a missing screen");
    for(const auto& object:document.at("screens").at(screen).at("obj"))if(object.at("oid")==oid)return object.at("base").value("name",std::string{});
    throw std::runtime_error("Variable connection references a missing widget");
}
lv_obj_t* Action(lv_obj_t* root,const char* action){
    if(const auto* value=EditorIcons::Action(root))if(std::string(value)==action)return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=Action(lv_obj_get_child(root,i),action))return found;
    return nullptr;
}
void Click(lv_obj_t* root,const char* caption,SdlSimulator& simulator){
    auto* label=Input::FindLabel(root,caption);Input::Require(label,std::string("Variable editor command is missing: ")+caption);
    auto* button=Input::ParentButton(label);Input::Require(button,"Variable editor caption has no button");Input::Click(button,simulator);
}
lv_obj_t* Field(lv_obj_t* root,const char* caption,const lv_obj_class_t* type=&lv_textarea_class){
    auto* field=Input::FindField(root,caption,type);Input::Require(field,std::string("Variable editor field is missing: ")+caption);return field;
}
std::string Token(std::string value){value.erase(std::remove_if(value.begin(),value.end(),[](unsigned char c){return !std::isalnum(c);}),value.end());for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
void Choose(lv_obj_t* control,const std::string& desired,SdlSimulator& simulator){
    const std::string options=lv_dropdown_get_options(control);std::vector<std::string> entries;size_t begin=0;
    for(;;){const auto end=options.find('\n',begin);entries.push_back(options.substr(begin,end==std::string::npos?end:end-begin));if(end==std::string::npos)break;begin=end+1;}
    size_t selected=entries.size();for(size_t i=0;i<entries.size();++i)if(Token(entries[i])==Token(desired)){selected=i;break;}
    if(selected==entries.size())for(size_t i=0;i<entries.size();++i)if(Token(entries[i]).find(Token(desired))!=std::string::npos){Input::Require(selected==entries.size(),"Ambiguous variable dropdown option");selected=i;}
    Input::Require(selected<entries.size(),"Variable dropdown omitted option: "+desired);
    Input::Click(control,simulator);Input::AssertFocused(control,simulator);
    const auto previous=lv_dropdown_get_selected(control);
    for(size_t step=0;step<(previous>selected?previous-selected:selected-previous);++step)
        Input::Key(previous>selected?SDLK_UP:SDLK_DOWN,previous>selected?SDL_SCANCODE_UP:SDL_SCANCODE_DOWN,simulator);
    Input::Key(SDLK_RETURN,SDL_SCANCODE_RETURN,simulator);
    Input::Require(lv_dropdown_get_selected(control)==selected,"Physical keyboard did not select variable dropdown option: "+desired);
}
lv_obj_t* Variables(UI& editor){
    auto* toolbar=editor.GetToolBar();Input::Require(toolbar,"Tools panel is missing");EditorWindow::Show(toolbar->GetWindow());
    lv_tabview_set_active(toolbar->GetTabView(),1,LV_ANIM_OFF);return toolbar->GetVariableTab();
}
void NamedSelection(UI& editor,const std::string& name,SdlSimulator& simulator){Click(Variables(editor),name.c_str(),simulator);Input::Require(std::string(lv_textarea_get_text(Field(Variables(editor),"Name")))==name,"Variable row selected another record");}
void UniqueNames(const json& document){std::set<std::string> names;for(const auto& item:document.at("variables")){const auto name=item.at("var").at("name").get<std::string>();Input::Require(!name.empty()&&names.insert(name).second,"Adding after deletion reused an existing variable name");}}
void References(const json& document,const std::string& expected,bool removed=false){
    size_t count=0,navigation=0;for(const auto& screen:document.at("screens"))for(const auto& record:screen.at("obj"))for(const auto& action:record.at("base").value("events",json::array())){
        if(action.value("action",std::string{})=="navigate")++navigation;
        if(action.contains("variable")){++count;Input::Require(!removed&&action.at("variable")==expected,"Variable edit left a stale action reference");}
        if(action.contains("condition")&&action.at("condition").contains("variable")){++count;Input::Require(!removed&&action.at("condition").at("variable")==expected,"Variable edit left a stale condition reference");}
    }
    Input::Require(navigation==1,"Variable edit removed an unrelated navigation action");Input::Require(count==(removed?0u:4u),"Variable edit lost dependent actions or conditions");
}
void AddBinding(UI& editor,SdlSimulator& simulator,bool selected,const char* screen,const char* widget,const char* property,const char* direction,const std::filesystem::path& capture={}){
    Click(Variables(editor),selected?"Use selected":"Add widget",simulator);
    auto* dialog=Input::WindowFromTitle("Add widget connection");Input::Require(dialog,"Add widget connection dialog did not open");
    Choose(Field(dialog,"Screen",&lv_dropdown_class),screen,simulator);
    Choose(Field(dialog,"Widget",&lv_dropdown_class),widget,simulator);
    Choose(Field(dialog,"Property",&lv_dropdown_class),property,simulator);
    Choose(Field(dialog,"Direction",&lv_dropdown_class),direction,simulator);
    if(!capture.empty())Capture(simulator,capture);
    Click(dialog,"Save connection",simulator);
    Input::Require(!Input::WindowFromTitle("Add widget connection"),"A valid variable connection was not saved");
}
void SaveOpen(UI& editor,SdlSimulator& simulator,const std::filesystem::path& path){
    const auto expected=editor.ProjectDocument();editor.MenuAction("save-as");auto* dialog=Input::WindowFromTitle("Save project");Input::Require(dialog,"Save project dialog did not open");
    Input::Replace(Input::FindClass(EditorWindow::Content(dialog),&lv_textarea_class),path.u8string(),simulator);Click(dialog,"Save",simulator);
    Input::Require(!Input::WindowFromTitle("Save project")&&!editor.Dirty(),"Variable project was not saved cleanly");
    json saved;{std::ifstream input(path);input>>saved;}Input::Require(saved.at("variables")==expected.at("variables"),"JSON save lost variable connections or code metadata");
    editor.MenuAction("new");Input::Require(editor.ProjectDocument().at("variables").empty(),"New project retained the previous variables");
    editor.MenuAction("open");dialog=Input::WindowFromTitle("Open project");Input::Require(dialog,"Open project dialog did not open");
    Input::Replace(Input::FindClass(EditorWindow::Content(dialog),&lv_textarea_class),path.u8string(),simulator);Click(dialog,"Open",simulator);
    Input::Require(!Input::WindowFromTitle("Open project")&&editor.ProjectDocument().at("variables")==expected.at("variables"),"Reopening JSON lost variable metadata");
    const auto reopened=editor.ProjectDocument();
    for(const auto& record:reopened.at("variables")){
        const auto variable=Serialization::LVVariable::FromJSON(record);const auto restored=Serialization::LVVariable::FromProtobuf(Serialization::LVVariable::ToProtobuf(variable));
        Input::Require(Serialization::LVVariable::ToJSON(restored)==Serialization::LVVariable::ToJSON(variable),"Variable Protobuf roundtrip lost connections or C/C++ mapping");
    }
}
void Validation(const json& valid){
    Input::Require(ScreenValidation::BlockingErrors(valid).empty(),"Valid multi-widget variable connections blocked export");
    auto broken=valid;auto& variable=broken["variables"][0]["var"];variable["bindings"].push_back({{"screen",1},{"oid",999},{"property","/text"},{"direction","to-widget"}});
    auto other=broken["variables"][0];other["var"]["name"]="other";other["var"].erase("code");other["var"]["bindings"]=json::array({{{"screen",1},{"oid",999},{"property","/text"},{"direction","to-widget"}}});
    // Use a different variable for the same missing object: a fix must never clear its connection.
    broken["variables"].push_back(other);bool found=false;
    for(const auto& issue:ScreenValidation::Analyze(broken))if(issue.variableName=="counter"&&issue.bindingIndex==2){
        const auto before=VarRecord(broken);Input::Require(!ScreenValidation::FixCaption(broken,issue).empty(),"Missing widget connection has no safe quick fix");
        Input::Require(ScreenValidation::ApplyFix(broken,issue),"Missing widget connection quick fix did not apply");
        const auto after=VarRecord(broken);Input::Require(after.at("bindings").size()==2&&after.at("code")==before.at("code")&&VarRecord(broken,"other").at("bindings").size()==1,"Quick fix removed valid connections, another variable, or code metadata");found=true;break;
    }
    Input::Require(found,"Validation omitted a specific broken multi-widget connection");
    auto unsupported=valid;unsupported["variables"][0]["var"]["bindings"][0]["property"]="/slider/unknown";Input::Require(!ScreenValidation::BlockingErrors(unsupported).empty(),"Unsupported connection property did not block export");
    auto unreadable=valid;unreadable["variables"][0]["var"]["bindings"][1]["direction"]="from-widget";Input::Require(!ScreenValidation::BlockingErrors(unreadable).empty(),"Unreadable text output accepted from-widget binding");
    auto collision=valid;other=valid["variables"][0];other["var"]["name"]="collision";other["var"].erase("code");other["var"]["bindings"]=json::array({other["var"]["bindings"][0]});collision["variables"].push_back(other);Input::Require(!ScreenValidation::BlockingErrors(collision).empty(),"Multiple writers to the same widget property did not block export");
    collision["variables"][0]["var"]["bindings"][0]["direction"]="from-widget";collision["variables"][1]["var"]["bindings"][0]["direction"]="from-widget";
    Input::Require(ScreenValidation::BlockingErrors(collision).empty(),"Read-only variables cannot share a widget source");
    auto narrow=valid;narrow["variables"][0]["var"]["code"]["cType"]="uint8_t";narrow["variables"][0]["var"]["default"]=300;Input::Require(!ScreenValidation::BlockingErrors(narrow).empty(),"C storage overflow did not block export");
    auto legacy=valid;legacy["variables"].push_back({{"var",{{"name","legacy"},{"dataType",1},{"default",0},{"oid",999},{"property","/slider/value"}}}});found=false;
    for(const auto& issue:ScreenValidation::Analyze(legacy))if(issue.variableName=="legacy"){Input::Require(ScreenValidation::ApplyFix(legacy,issue),"Legacy broken binding could not be cleared");found=true;break;}
    Input::Require(found&&VarRecord(legacy,"legacy").at("bindings").empty()&&ScreenValidation::BlockingErrors(legacy).empty(),"Legacy quick fix left an active or invalid binding");
}
void RuntimePreview(UI& editor,SdlSimulator& simulator){
    const auto before=editor.ProjectDocument();editor.MenuAction("run-toggle");Input::Require(editor.Running(),"Bound Variables fixture did not start Run preview");
    auto* runtime=editor.RunRuntime();lvgl_runtime_value_t value{};value.type=LVGL_RUNTIME_INTEGER;value.integer_value=42;
    Input::Require(lvgl_runtime_set_variable(runtime,"counter",&value),"Run preview rejected a variable update");Input::Require(lvgl_runtime_ensure_screen(runtime,1),"Run preview could not load a connected second screen");
    auto* slider=lvgl_runtime_object(runtime,0,WidgetId(before,0,"Main slider"));auto* label=lvgl_runtime_object(runtime,1,WidgetId(before,1,"Other readout"));
    Input::Require(slider&&label&&lv_slider_get_value(slider)==42&&std::string(lv_label_get_text(label))=="42","Variable update did not reach all connected widgets including a lazy screen");
    lv_slider_set_value(slider,19,LV_ANIM_OFF);lv_obj_send_event(slider,LV_EVENT_VALUE_CHANGED,nullptr);Input::Drain(simulator);
    Input::Require(lvgl_runtime_get_variable(runtime,"counter",&value)&&value.integer_value==19&&std::string(lv_label_get_text(label))=="19","Widget input did not update its variable and connected peers");
    editor.MenuAction("run-stop");Input::Require(!editor.Running()&&editor.ProjectDocument()==before,"Run preview changed authored variable defaults or bindings");
}
void Screens(UI& editor){
    const auto before=editor.ProjectDocument();editor.MoveScreen(1,0);auto doc=editor.ProjectDocument();
    const auto moved=VarRecord(doc).at("bindings");Input::Require(TargetName(doc,moved[0])=="Main slider"&&TargetName(doc,moved[1])=="Other readout","Screen move connected the variable to different widgets");
    Input::Require(editor.Undo()&&editor.ProjectDocument()==before,"Screen move did not preserve variable connections through Undo");
    const auto copied=editor.DuplicateScreen(0);doc=editor.ProjectDocument();const auto bindings=VarRecord(doc).at("bindings");
    Input::Require(doc.at("variables").size()==1&&bindings.size()==3&&bindings[2].at("screen")==copied&&TargetName(doc,bindings[2])=="Main slider","Duplicating a screen did not extend the shared variable's connections");
    Input::Require(editor.Undo()&&editor.ProjectDocument()==before,"Screen duplication did not restore exact variable bindings through Undo");
    editor.DeleteScreen(0);doc=editor.ProjectDocument();const auto remaining=VarRecord(doc).at("bindings");Input::Require(remaining.size()==1&&remaining[0].at("screen")==0&&TargetName(doc,remaining[0])=="Other readout","Deleting one screen removed unrelated variable connections");
    Input::Require(editor.Undo()&&editor.ProjectDocument()==before,"Screen deletion did not restore variable connections through Undo");
}
void Capture(SdlSimulator& simulator,const std::filesystem::path& path){Input::Drain(simulator);auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));auto* image=SDL_RenderReadPixels(renderer,nullptr);Input::Require(image,"Could not capture Variables editor");const bool saved=SDL_SaveBMP(image,path.u8string().c_str());SDL_DestroySurface(image);Input::Require(saved,"Could not save Variables editor capture");}
}
void CheckVariablesEditor(UI& editor,SdlSimulator& simulator,const std::filesystem::path& outputDirectory){
    ClipboardRestore restoreClipboard;
    std::cout<<"Variables editor: setup and rename"<<std::endl;
    const auto initial=editor.ProjectDocument();const auto work=std::filesystem::absolute(outputDirectory/ ("run-"+std::to_string(SDL_GetTicksNS())));std::filesystem::create_directories(work);editor.EnableSessionPersistence(work/"session");
    editor.ApplyDocument(Fixture());auto* tab=Variables(editor);Input::Drain(simulator);
    Input::Replace(Field(tab,"Name"),"counter",simulator);References(editor.ProjectDocument(),"counter");
    std::cout<<"Variables editor: typed defaults"<<std::endl;
    for(const auto& item:std::vector<std::pair<std::string,json>>{{"String",u8"été"},{"Float",1.25},{"Boolean",true},{"DateTime",INT64_C(1770000000)},{"Integer",7}}){
        Choose(Field(tab,"Type",&lv_dropdown_class),item.first,simulator);Input::Replace(Field(tab,"Default"),item.second.is_string()?item.second.get<std::string>():item.second.dump(),simulator);
        Input::Require(VarRecord(editor.ProjectDocument()).at("default")==item.second,"Typed variable default was not stored with the selected type");
    }
    std::cout<<"Variables editor: C/C++ mapping"<<std::endl;
    Input::Replace(Field(tab,"History"),"4",simulator);Input::Replace(Field(tab,"C/C++ symbol"),"shared_count",simulator);Choose(Field(tab,"C type",&lv_dropdown_class),"int32_t",simulator);
    auto authored=VarRecord(editor.ProjectDocument());Input::Require(authored.at("history")==4&&authored.at("code").at("symbol")=="shared_count"&&authored.at("code").at("cType")=="int32_t","Variables editor lost typed C/C++ mapping");
    Click(tab,"Copy wiring code",simulator);
    char* clipboard=SDL_GetClipboardText();Input::Require(clipboard,"Copy wiring code did not provide clipboard text");
    const std::string wiring=clipboard;SDL_free(clipboard);
    Input::Require(wiring.find("ui_bind_v_counter(&shared_count)")!=std::string::npos,"Copied wiring code did not bind the existing C/C++ symbol");
    Input::Require(wiring.find("static int32_t shared_count")==std::string::npos,"Copied wiring code shadows the existing C/C++ symbol with new storage");
    Input::Replace(Field(tab,"Default"),"invalid",simulator);Input::Require(VarRecord(editor.ProjectDocument()).at("default")==7,"An invalid typed draft corrupted the saved variable default");Input::Replace(Field(tab,"Default"),"7",simulator);
    std::cout<<"Variables editor: unique names"<<std::endl;
    auto add=[&]{Input::Click(Action(tab,"variable-add"),simulator);return editor.ProjectDocument().at("variables").back().at("var").at("name").get<std::string>();};
    const auto first=add(),second=add();
    const auto beforeSelection=editor.ProjectDocument();const auto selectionRevision=editor.DocumentRevision();const auto selectionHistory=editor.HistoryLogicalBytes();
    NamedSelection(editor,first,simulator);Input::Require(editor.ProjectDocument()==beforeSelection&&editor.DocumentRevision()==selectionRevision&&editor.HistoryLogicalBytes()==selectionHistory,"Selecting a variable created an authored edit or history entry");
    Input::Replace(Field(tab,"Default"),"13",simulator);NamedSelection(editor,second,simulator);Input::Replace(Field(tab,"Default"),"17",simulator);
    Input::Require(editor.Undo()&&VarRecord(editor.ProjectDocument(),first).at("default")==13&&VarRecord(editor.ProjectDocument(),second).at("default")==0,"Undo coalesced edits made to different variables");
    Input::Require(editor.Undo()&&editor.ProjectDocument()==beforeSelection,"Separate variable edits were not independently undoable");
    NamedSelection(editor,first,simulator);Input::Click(Action(tab,"variable-delete"),simulator);const auto third=add();UniqueNames(editor.ProjectDocument());
    NamedSelection(editor,second,simulator);Input::Click(Action(tab,"variable-delete"),simulator);NamedSelection(editor,third,simulator);Input::Click(Action(tab,"variable-delete"),simulator);NamedSelection(editor,"counter",simulator);
    std::cout<<"Variables editor: connections"<<std::endl;
    auto doc=editor.ProjectDocument();editor.SelectObjectOnScreen(0,WidgetId(doc,0,"Main slider"));AddBinding(editor,simulator,true,"Main","Main slider","Value","Read and write",outputDirectory/"variables-binding.bmp");
    AddBinding(editor,simulator,false,"Other","Other readout","Text","Display variable");doc=editor.ProjectDocument();auto bindings=VarRecord(doc).at("bindings");
    Input::Require(bindings.size()==2&&TargetName(doc,bindings[0])=="Main slider"&&TargetName(doc,bindings[1])=="Other readout"&&bindings[0].at("direction")=="two-way"&&bindings[1].at("direction")=="to-widget","Variables panel did not save independent widget targets/directions");
    std::cout<<"Variables editor: connection edits and Undo/Redo"<<std::endl;
    Click(tab,"Edit",simulator);auto* edit=Input::WindowFromTitle("Edit widget connection");Input::Require(edit,"Edit connection dialog did not open");Choose(Field(edit,"Direction",&lv_dropdown_class),"Input to variable",simulator);Click(edit,"Save connection",simulator);
    Input::Require(VarRecord(editor.ProjectDocument()).at("bindings")[0].at("direction")=="from-widget","Connection direction edit was not saved");Input::Require(editor.Undo()&&editor.ProjectDocument()==doc,"Connection edit was not exactly undoable");
    Click(tab,"Remove",simulator);Input::Require(VarRecord(editor.ProjectDocument()).at("bindings").size()==1,"Remove connection did not target one row");Input::Require(editor.Undo()&&editor.ProjectDocument()==doc,"Removing a connection was not exactly undoable");
    Input::Require(editor.Redo()&&VarRecord(editor.ProjectDocument()).at("bindings").size()==1,"Removed connection was not redoable");Input::Require(editor.Undo()&&editor.ProjectDocument()==doc,"Redo of a removed connection did not remain undoable");
    std::cout<<"Variables editor: validation"<<std::endl;Validation(editor.ProjectDocument());
    std::cout<<"Variables editor: persistence"<<std::endl;SaveOpen(editor,simulator,work/u8"variables 日本.json");
    std::cout<<"Variables editor: Run preview"<<std::endl;RuntimePreview(editor,simulator);
    std::cout<<"Variables editor: screen operations"<<std::endl;Screens(editor);
    doc=editor.ProjectDocument();editor.SelectObjectOnScreen(0,WidgetId(doc,0,"Main slider"));editor.MenuAction("delete");Input::Require(VarRecord(editor.ProjectDocument()).at("bindings").size()==1,"Deleting a widget did not prune only its connection");Input::Require(editor.Undo()&&editor.ProjectDocument()==doc,"Deleting a connected widget was not exactly undoable");
    NamedSelection(editor,"counter",simulator);Input::Click(Action(tab,"variable-delete"),simulator);Input::Require(editor.ProjectDocument().at("variables").empty(),"Delete variable did not remove its record");References(editor.ProjectDocument(),"counter",true);Input::Require(editor.Undo()&&editor.ProjectDocument()==doc,"Variable deletion/action cleanup was not exactly undoable");
    NamedSelection(editor,"counter",simulator);std::filesystem::create_directories(outputDirectory);
    lv_obj_scroll_to_view_recursive(Field(tab,"Type",&lv_dropdown_class),LV_ANIM_OFF);Capture(simulator,outputDirectory/"variables-default.bmp");
    if(auto* connections=Input::FindLabel(tab,"Widget connections")){lv_obj_scroll_to_view_recursive(connections,LV_ANIM_OFF);Capture(simulator,outputDirectory/"variables-default-connections.bmp");}
    auto* tools=editor.GetToolBar()->GetWindow();const int x=lv_obj_get_x(tools),y=lv_obj_get_y(tools),width=lv_obj_get_width(tools),height=lv_obj_get_height(tools);
    lv_obj_set_pos(tools,300,90);lv_obj_set_size(tools,840,860);lv_obj_move_foreground(tools);
    lv_obj_scroll_to_view_recursive(Field(tab,"Type",&lv_dropdown_class),LV_ANIM_OFF);Capture(simulator,outputDirectory/"variables-editor.bmp");
    if(auto* connections=Input::FindLabel(tab,"Widget connections")){lv_obj_scroll_to_view_recursive(connections,LV_ANIM_OFF);Capture(simulator,outputDirectory/"variables-connections.bmp");}
    lv_obj_set_pos(tools,x,y);lv_obj_set_size(tools,width,height);
    {std::ofstream output(outputDirectory/"variables-editor.json");output<<editor.ProjectDocument().dump(2);Input::Require(bool(output),"Could not write Variables verification fixture");}
    editor.ApplyDocument(initial);std::cout<<"Variables: real typed defaults/code mapping, unique names, multi-widget directions, JSON/Protobuf, validation/precise quick fixes, Run fanout, screens, target deletion and Undo passed\n";
}
