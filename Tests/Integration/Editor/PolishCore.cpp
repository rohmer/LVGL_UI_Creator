#include "PolishCore.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/CommandPalette.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Screens/ScreenFlowWindow.h"
#include <Serialization/ProjectScreens.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) { if(!condition) throw std::runtime_error(message); }
lv_obj_t* Label(lv_obj_t* root, const char* text) {
    if(lv_obj_has_class(root,&lv_label_class) && std::string(lv_label_get_text(root))==text) return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i) if(auto* found=Label(lv_obj_get_child(root,i),text)) return found;
    return nullptr;
}
lv_obj_t* SearchField(lv_obj_t* root) {
    if(lv_obj_has_class(root,&lv_textarea_class)) {
        const auto* placeholder=lv_textarea_get_placeholder_text(root);
        if(placeholder && std::string(placeholder)=="Type to search...") return root;
    }
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i) if(auto* found=SearchField(lv_obj_get_child(root,i))) return found;
    return nullptr;
}
lv_obj_t* FlowControl(const char* caption,const lv_obj_class_t* type) {
    Require(ScreenFlowWindow::ActiveWindow(),"Screen flow closed during external editing");
    auto* label=Label(ScreenFlowWindow::ActiveWindow(),caption);Require(label,"Screen flow field caption is missing");
    auto* control=lv_obj_get_child(lv_obj_get_parent(label),lv_obj_get_index(label)+1);
    Require(control&&lv_obj_has_class(control,type),"Screen flow field control is missing");return control;
}
void ClickFlow(const char* caption) {
    auto* label=Label(ScreenFlowWindow::ActiveWindow(),caption);Require(label,"Screen flow action is missing");
    lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);
}
json Read(const std::filesystem::path& path) { std::ifstream input(path);Require(bool(input),"Saved project is missing");return json::parse(input); }
void SaveAs(UI& editor, const std::filesystem::path& path) {
    editor.MenuAction("save-as");auto* title=Label(lv_layer_top(),"Save project");Require(title,"Save As did not open its path dialog");
    auto* dialog=lv_obj_get_parent(lv_obj_get_parent(title));auto* field=lv_obj_get_child(EditorWindow::Content(dialog),0);
    Require(lv_obj_has_class(field,&lv_textarea_class),"Save As path field is missing");
    lv_textarea_set_text(field,path.u8string().c_str());editor.MenuAction("confirm-file");
    Require(!Label(lv_layer_top(),"Save project"),"Save As failed to close after writing");
}
json Widget(int id,const char* type,const char* caption,int y) {
    json record={{"oid",id},{"par",-1},{"base",{{"name",caption},{"drag",true},{"coords",{{"x",16},{"y",y},{"width",180},{"height",48}}}}}};
    record[type]=std::string(type)=="text"?json(caption):json{{"text",caption}};return record;
}
json ScreensFixture() {
    auto home=Widget(0,"button","Open settings",16);
    home["base"]["events"]=json::array({{{"event","clicked"},{"action","navigate"},{"screen",1}},{{"event","clicked"},{"action","hide"},{"targetScreen",1},{"targetOid",0}}});
    auto counter=Widget(0,"text","0",16),increment=Widget(1,"button","Increment",80);
    increment["base"]["events"]=json::array({{{"event","clicked"},{"action","increment_variable"},{"variable","count"},{"value",1},{"condition",{{"variable","count"},{"operator","less"},{"value",10}}}},{{"event","clicked"},{"action","navigate"},{"screen",1}}});
    json document={{"screens",json::array({{{"id","home"},{"name","Home"},{"canvas",{{"width",320},{"height",240}}},{"obj",json::array({home})}},{{"id","settings"},{"name","Settings"},{"canvas",{{"width",320},{"height",240}}},{"obj",json::array({counter,increment})}}})},{"activeScreen",1},{"editorScreen",0},
        {"variables",json::array({{{"var",{{"name","count"},{"dataType",1},{"default",0},{"oid",0},{"screen",1},{"property","text"}}}}})}};
    Serialization::ProjectScreens::Select(document,0);return document;
}
const json& FindVariable(const json& document,const char* name) {
    for(const auto& item:document.at("variables")) if(item.at("var").value("name",std::string{})==name) return item.at("var");
    throw std::runtime_error(std::string("Missing variable: ")+name);
}
void CheckPatchHistory() {
    const std::string asset(1024*1024,'q');UndoHistory history(100,16*1024*1024);
    auto state=[&](int value){return UndoHistory::State{"{\"asset\":\""+asset+"\",\"value\":"+std::to_string(value)+"}",value,{value},size_t(value%3)};};
    history.Reset(state(0));for(int i=1;i<=40;++i) Require(history.Record(state(i)),"Patch history missed an edit");
    Require(history.StorageBytes()*10<history.LogicalBytes(),"Small edits copied the shared megabyte asset into every undo entry");
    for(int i=39;i>=0;--i){Require(history.UndoState().document==state(i).document && history.UndoState().screen==size_t(i%3),"Undo patch did not restore content and screen");history.DidUndo();}
    for(int i=1;i<=40;++i){Require(history.RedoState().document==state(i).document,"Redo patch did not restore content");history.DidRedo();}
    history.DidUndo();Require(history.Record(state(99))&&!history.CanRedo(),"Editing after undo retained an invalid redo branch");
    UndoHistory limited(2,16*1024*1024);limited.Reset(state(0));for(int i=1;i<5;++i)limited.Record(state(i));
    Require(limited.Size()==3,"Undo history entry budget was not enforced");Require(limited.UndoState().document==state(3).document,"Trimming broke the remaining undo baseline");limited.DidUndo();Require(limited.UndoState().document==state(2).document,"Trimmed patch chain is incorrect");
}
}

void CheckPolishCore(UI& editor,SdlSimulator& simulator) {
    CheckPatchHistory();editor.ApplyDocument(ScreensFixture());
    const auto path=std::filesystem::absolute("polish-core-project.json");SaveAs(editor,path);Require(!editor.Dirty(),"Saving did not establish a clean multi-screen document");
    const auto canonical=Serialization::ProjectScreens::Canonical(editor.ProjectDocument());const auto historyBytes=editor.HistoryLogicalBytes();const bool couldUndo=editor.CanUndo();
    editor.SelectObjectIds({0});lv_obj_update_layout(lv_screen_active());simulator.ProcessEvents();
    Require(editor.SelectedObjectIds()==std::vector<int>{0},"Saved-screen widget selection did not select the authored control");
    Require(!editor.Dirty()&&Serialization::ProjectScreens::Canonical(editor.ProjectDocument())==canonical,"Selecting a widget immediately after Save changed or dirtied the authored document");
    Require(editor.HistoryLogicalBytes()==historyBytes&&editor.CanUndo()==couldUndo,"Widget selection immediately after Save added an undo entry");
    Require(editor.SelectScreen(1)&&editor.ActiveScreenIndex()==1&&editor.SelectScreen(0),"Read-only screen selection failed");
    Require(!editor.SelectScreen(99),"Selecting a missing screen succeeded");
    Require(!editor.Dirty()&&editor.HistoryLogicalBytes()==historyBytes&&editor.CanUndo()==couldUndo,"Browsing screens made the project dirty or added an undo entry");
    Require(Serialization::ProjectScreens::Canonical(editor.ProjectDocument())==canonical,"Browsing screens changed authored content");
    // An open flow window follows screen selection, external widget edits and undo.
    editor.MenuAction("screen-flow");Require(ScreenFlowWindow::ActiveWindow(),"Screen flow window did not open");
    Require(std::string(lv_textarea_get_text(FlowControl("Screen name",&lv_textarea_class)))=="Home","Screen flow did not start on the selected screen");
    Require(editor.SelectScreen(1),"External screen selection failed while flow was open");
    Require(std::string(lv_textarea_get_text(FlowControl("Screen name",&lv_textarea_class)))=="Settings","Screen flow retained another screen's name after dock selection");
    Require(std::string(lv_dropdown_get_options(FlowControl("Add click navigation from widget",&lv_dropdown_class)))=="0\nIncrement","Screen flow retained another screen's source widgets");
    lv_textarea_set_text(FlowControl("Screen name",&lv_textarea_class),"Flow preferences");ClickFlow("Save screen properties");
    auto flowDocument=editor.ProjectDocument();
    Require(flowDocument["screens"][1]["name"]=="Flow preferences"&&flowDocument["screens"][0]["name"]=="Home","Screen flow saved properties to a screen other than the selected page");
    flowDocument["obj"][1]["base"]["name"]="Updated control";editor.ApplyDocument(std::move(flowDocument));
    Require(std::string(lv_dropdown_get_options(FlowControl("Add click navigation from widget",&lv_dropdown_class))).find("Updated control")!=std::string::npos,"Screen flow did not refresh after an external widget edit");
    Require(editor.Undo(),"External widget edit was not undoable while flow was open");
    Require(std::string(lv_dropdown_get_options(FlowControl("Add click navigation from widget",&lv_dropdown_class)))=="0\nIncrement","Screen flow did not refresh the widget list after undo");
    Require(editor.Undo(),"Screen flow property edit was not undoable");
    Require(std::string(lv_textarea_get_text(FlowControl("Screen name",&lv_textarea_class)))=="Settings","Screen flow did not refresh its name after undo");
    ScreenFlowWindow::Close();Require(editor.SelectScreen(0),"Could not restore the first screen after flow review");

    editor.RenameScreen(1,"Preferences");const auto copy=editor.DuplicateScreen(1);auto document=editor.ProjectDocument();
    Require(copy==2&&editor.ScreenCount()==3&&document["screens"][copy]["name"]=="Preferences copy","Rename/duplicate did not create the expected screen");
    Require(document["screens"][copy]["id"]!=document["screens"][1]["id"],"Duplicate reused a stable screen ID");
    Require(FindVariable(document,"count_copy")["screen"]==2,"Duplicate did not clone the screen-bound variable");
    const auto& copiedActions=document["screens"][2]["obj"][1]["base"]["events"];
    Require(copiedActions[0]["variable"]=="count_copy"&&copiedActions[0]["condition"]["variable"]=="count_copy"&&copiedActions[1]["screen"]==2,"Duplicate retained another screen's local variable or self-navigation references");
    editor.MoveScreen(2,0);document=editor.ProjectDocument();
    Require(document["screens"][0]["name"]=="Preferences copy"&&document["screens"][1]["id"]=="home"&&document["activeScreen"]==2,"Reordering lost stable screen identity or the start screen");
    Require(FindVariable(document,"count")["screen"]==2&&FindVariable(document,"count_copy")["screen"]==0,"Reordering did not remap variable bindings");
    Require(document["screens"][1]["obj"][0]["base"]["events"][0]["screen"]==2&&document["screens"][1]["obj"][0]["base"]["events"][1]["targetScreen"]==2,"Reordering did not remap navigation and explicit object targets");
    editor.DeleteScreen(2);document=editor.ProjectDocument();
    Require(editor.ScreenCount()==2&&FindVariable(document,"count")["oid"]==-1&&!FindVariable(document,"count").contains("screen"),"Deleting a screen left a dangling variable binding");
    Require(document["screens"][1]["obj"][0]["base"]["events"].empty(),"Deleting a screen left dangling navigation or object actions");
    Require(editor.Undo()&&editor.ScreenCount()==3&&FindVariable(editor.ProjectDocument(),"count")["screen"]==2,"Deleting a screen was not fully undoable");
    const auto added=editor.AddScreen("Diagnostics");Require(added==3&&editor.ActiveScreenIndex()==3&&editor.ProjectDocument()["obj"].empty(),"Add screen did not select a fresh canvas");
    editor.DeleteScreen(3);editor.DeleteScreen(2);editor.DeleteScreen(1);bool rejected=false;try{editor.DeleteScreen(0);}catch(const std::invalid_argument&){rejected=true;}Require(rejected&&editor.ScreenCount()==1,"The final screen could be deleted");

    // Save must update the existing path; Save As remains a deliberate path choice.
    editor.RenameScreen(0,"Saved directly");Require(editor.HandleShortcut('s',true),"Ctrl+S was not handled");
    Require(!Label(lv_layer_top(),"Save project")&&!editor.Dirty(),"Ctrl+S opened a path dialog instead of saving the known project");
    Require(Read(path)["screens"][0]["name"]=="Saved directly","Ctrl+S did not write the current document");
    Require(editor.HandleShortcut('s',true,true)&&Label(lv_layer_top(),"Save project"),"Ctrl+Shift+S did not open Save As");editor.MenuAction("cancel-file");

    const auto beforeCommands=editor.ScreenCount();Require(editor.HandleShortcut('p',true,true)&&CommandPalette::ActiveWindow(),"Command palette shortcut did not route to the palette");
    auto* query=SearchField(CommandPalette::ActiveWindow());Require(query,"Command palette query is missing");lv_textarea_set_text(query,"Add screen");Require(CommandPalette::ResultCount()==1,"Command palette did not filter the intended action");
    SDL_Event enter{};enter.type=SDL_EVENT_KEY_DOWN;enter.key.key=SDLK_RETURN;Require(CommandPalette::HandleEvent(enter),"Command palette did not handle Enter");
    Require(!CommandPalette::ActiveWindow()&&editor.ScreenCount()==beforeCommands+1,"Command palette did not invoke Add screen");
    Require(editor.SelectScreen(0),"Could not select a screen with authored controls");editor.SelectObjectIds({1});
    Require(editor.HandleShortcut('f',true,true)&&CommandPalette::ActiveWindow(),"Find property shortcut did not open the property palette");
    query=SearchField(CommandPalette::ActiveWindow());Require(query,"Property query is missing");lv_textarea_set_text(query,"Width");Require(CommandPalette::ResultCount()>0,"Find property did not expose the selected widget's width");
    Require(CommandPalette::HandleEvent(enter)&&!CommandPalette::ActiveWindow(),"Property search did not reveal and focus the chosen inspector control");
    auto* focused=lv_group_get_focused(lv_indev_get_group(simulator.GetKeyboard()));Require(focused&&lv_obj_has_class(focused,&lv_textarea_class),"Property search did not focus an editable inspector field");

    editor.ProjectDocument();editor.Dirty();const auto serializations=editor.SerializationCount();const auto end=SDL_GetTicks()+1200;
    while(SDL_GetTicks()<end){SDL_Delay(5);simulator.ProcessEvents();editor.ProjectDocument();editor.Dirty();}
    Require(editor.SerializationCount()==serializations,"Idle timers repeatedly serialized an unchanged project");
    const auto revision=editor.DocumentRevision();editor.ApplyMetadata({{"accessibility",{{"minimumContrast",7.0}}}});Require(editor.DocumentRevision()>revision,"Metadata editing did not invalidate the document revision");
    editor.ProjectDocument();const auto refreshed=editor.SerializationCount();editor.ProjectDocument();editor.Dirty();Require(refreshed>serializations&&editor.SerializationCount()==refreshed,"Edited documents were not rebuilt once and cached");
    std::cout<<"Multi-screen CRUD/reference remapping, read-only navigation, direct Save/Save As, command/property routing, patch undo and revision-backed document cache passed\n";
}
