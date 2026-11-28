#include "HierarchyIcons.h"
#include "EditorInputTest.h"
#include "../../../UICreator/Utils/ObjectIcons.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../3rdParty/LVGL/lvgl/include/lvgl/widgets/lv_bootstrap.h"
#include <Serialization/ObjectSerializer.h>
#include <Serialization/ProjectDesign.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace Input=EditorInputTest;
struct Model {ToolTray* tray;TreeView* tree;PropertyWindow* properties;};
Model CurrentModel(){
    auto* preview=Input::WindowFromTitle("Simulator [Size:320x240]");Input::Require(preview,"Simulator is missing");
    auto* canvas=lv_obj_get_child(EditorWindow::Content(preview),0);auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(canvas));
    Input::Require(data&&data->toolTray,"Canvas has no authoring model");auto* properties=data->toolTray->GetPropertyWindow();
    return {data->toolTray,properties->GetObjectTree(),properties};
}
lv_obj_t* Named(TreeView* tree,const std::string& name){
    for(auto* object:tree->GetObjects()){const auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));if(data&&data->objectName==name)return object;}
    throw std::runtime_error("Hierarchy object is missing: "+name);
}
bool Hidden(lv_obj_t* object){for(auto* parent=object;parent;parent=lv_obj_get_parent(parent))if(lv_obj_is_hidden(parent))return true;return false;}
TreeNode* CheckRow(Model model,lv_obj_t* object,const ObjectIcons::Icon* expected){
    Input::Require(ObjectIcons::ForObject(object)==expected,std::string(expected->key)+": object resolved to the wrong hierarchy icon");
    auto* node=model.tree->GetNodeByObject(object);Input::Require(node,"Authored object has no hierarchy node");
    auto* row=node->GetRowObject();auto* icon=node->GetIconObject();
    Input::Require(row&&icon&&lv_obj_get_parent(icon)==row&&ObjectIcons::Get(icon)==expected,"Hierarchy row lost its object-specific icon");
    Input::Require(lv_obj_get_width(icon)==24&&lv_obj_get_height(icon)==24,"Hierarchy icon must use its dedicated 24px size");
    Input::Require(!lv_obj_is_clickable(icon)&&!lv_obj_is_click_focusable(icon),"Hierarchy decoration intercepts selection or keyboard focus");
    lv_obj_t* caption=nullptr;for(uint32_t i=0;i<lv_obj_get_child_count(row);++i){auto* child=lv_obj_get_child(row,i);if(lv_obj_has_class(child,&lv_label_class)){Input::Require(!caption,"Hierarchy row has multiple direct caption labels");caption=child;}}
    Input::Require(caption,"Hierarchy caption must remain a direct row child");
    lv_obj_update_layout(lv_screen_active());lv_area_t ib{},cb{},rb{};lv_obj_get_coords(icon,&ib);lv_obj_get_coords(caption,&cb);lv_obj_get_coords(row,&rb);
    Input::Require(ib.x1>=rb.x1&&ib.x2<=rb.x2&&ib.y1>=rb.y1&&ib.y2<=rb.y2,"Hierarchy icon is clipped by its row");
    Input::Require(ib.x2<cb.x1&&cb.x2<=rb.x2,"Hierarchy icon and caption overlap in a narrow or nested row");
    Input::Require(cb.y1>=rb.y1&&cb.y2<=rb.y2,"Hierarchy caption is clipped vertically by its row");
    Input::Require(lv_obj_get_height(caption)<2*lv_font_get_line_height(lv_obj_get_style_text_font(caption,LV_PART_MAIN)),"Hierarchy caption wraps instead of using one line with ellipsis");
    for(uint32_t i=0;i<lv_obj_get_child_count(row);++i){auto* child=lv_obj_get_child(row,i);if(lv_obj_has_class(child,&lv_button_class)){lv_area_t disclosure{};lv_obj_get_coords(child,&disclosure);Input::Require(disclosure.x2<ib.x1,"Disclosure button overlaps the object icon");}}
    return node;
}
void SaveOpen(UI& editor,const std::filesystem::path& path){
    auto pathField=[](const char* title){auto* dialog=Input::WindowFromTitle(title);Input::Require(dialog,"Project path dialog is missing");auto* field=lv_obj_get_child(EditorWindow::Content(dialog),0);Input::Require(lv_obj_has_class(field,&lv_textarea_class),"Project path field is missing");return field;};
    editor.MenuAction("save-as");lv_textarea_set_text(pathField("Save project"),path.u8string().c_str());editor.MenuAction("confirm-file");
    Input::Require(std::filesystem::is_regular_file(path)&&!editor.Dirty(),"Hierarchy fixture was not saved cleanly");
    editor.MenuAction("open");lv_textarea_set_text(pathField("Open project"),path.u8string().c_str());editor.MenuAction("confirm-file");
    Input::Require(!Input::WindowFromTitle("Open project"),"Saved hierarchy fixture did not reopen");
}
void Capture(SdlSimulator& simulator,const std::filesystem::path& path){
    if(path.empty())return;Input::Drain(simulator);auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
    auto* image=SDL_RenderReadPixels(renderer,nullptr);Input::Require(image,"Cannot capture hierarchy icons");const bool saved=SDL_SaveBMP(image,path.u8string().c_str());SDL_DestroySurface(image);Input::Require(saved,"Cannot save hierarchy icon preview");
}
void NativeTypes(UI& editor,SdlSimulator& simulator,Model model,const std::filesystem::path& directory){
    auto* palette=EditorWindow::Content(Input::WindowFromTitle("Tool Tray"));Input::Require(palette,"Widget palette is missing");std::map<std::string,int> kinds;
    for(int id=0;id<34;++id){
        editor.MenuAction("select-screen");const auto* descriptor=ObjectIcons::Widget(id);auto* caption=Input::FindLabel(palette,descriptor->name);Input::Require(caption,"Native palette entry is missing");
        lv_obj_send_event(lv_obj_get_parent(caption),LV_EVENT_CLICKED,nullptr);auto* object=model.properties->GetSelectedObject();CheckRow(model,object,descriptor);
        kinds.emplace(static_cast<ObjectUserData*>(lv_obj_get_user_data(object))->objectName,id);
    }
    auto document=editor.ProjectDocument();Input::Require(document["obj"].size()==34,"Native hierarchy fixture lost or added authored widgets");
    // Imported projects have no palette marker. Misleading names must never replace
    // actual widget identity, including scale variants and composite controls.
    for(auto& record:document["obj"]){const auto id=kinds.at(record["base"]["name"].get<std::string>());record["base"]["name"]="Imported native "+std::to_string(id);
        if(record["base"].contains("design"))record["base"]["design"].erase("paletteIcon");
        if(id==0)record["base"]["design"]["paletteIcon"]="unknown.icon";
        if(id==1)record["base"]["design"]["paletteIcon"]=17;
    }
    editor.ApplyDocument(std::move(document));
    for(int id=0;id<34;++id)CheckRow(model,Named(model.tree,"Imported native "+std::to_string(id)),ObjectIcons::Widget(id));
    SaveOpen(editor,directory/"native-icons.json");
    for(int id=0;id<34;++id)CheckRow(model,Named(model.tree,"Imported native "+std::to_string(id)),ObjectIcons::Widget(id));
    auto* arc=Named(model.tree,"Imported native 0");Input::Require(model.tree->RenameObject(arc,"QR code"),"Hierarchy rename failed");CheckRow(model,arc,ObjectIcons::Widget(0));
    Input::Require(editor.Undo(),"Native hierarchy rename was not undoable");CheckRow(model,Named(model.tree,"Imported native 0"),ObjectIcons::Widget(0));
    Capture(simulator,directory/"native-hierarchy.bmp");
}
size_t Count(Model model,const ObjectIcons::Icon* descriptor){size_t count=0;for(auto* object:model.tree->GetObjects())if(ObjectIcons::ForObject(object)==descriptor){CheckRow(model,object,descriptor);++count;}return count;}
void PresetTypes(UI& editor,Model model,const json& empty,const std::filesystem::path& directory){
    for(int preset=0;preset<LV_BOOTSTRAP_PRESET_COUNT;++preset){
        editor.ApplyDocument(empty);editor.MenuAction("select-screen");const auto* expected=ObjectIcons::Preset(preset);auto* root=model.tray->CreateBootstrapPreset(preset);CheckRow(model,root,expected);
        const auto record=Serialization::ObjectSerializer::SerializeObject(root);
        Input::Require(record.at("base").at("design").at("paletteIcon")==expected->key,"Native recipe omitted stable icon provenance");
        if(preset!=LV_BOOTSTRAP_BUTTON&&preset!=LV_BOOTSTRAP_DASHBOARD&&preset!=LV_BOOTSTRAP_COMPONENT_GALLERY)continue;
        const auto name="Renamed recipe "+std::to_string(preset);Input::Require(model.tree->RenameObject(root,name),"Preset root could not be renamed");CheckRow(model,root,expected);
        auto bytes=Serialization::ObjectSerializer::ToProtobuf(root);auto* restored=Serialization::ObjectSerializer::FromProtobuf(bytes);
        Input::Require(ObjectIcons::ForObject(restored)==expected,"Widget Protobuf roundtrip lost recipe icon provenance");lv_obj_delete(restored);
        auto device=editor.ProjectDocument();Serialization::ProjectDesign::Materialize(device,true);
        for(const auto& object:device["obj"])Input::Require(!object.at("base").contains("design"),"Hierarchy icon metadata leaked into the device document");
        SaveOpen(editor,directory/("preset-"+std::to_string(preset)+".json"));root=Named(model.tree,name);CheckRow(model,root,expected);
        const auto objects=model.tree->GetObjects();const auto selected=std::find(objects.begin(),objects.end(),root);Input::Require(selected!=objects.end(),"Restored recipe is absent from the authoring object list");
        editor.SelectObjectIds({int(std::distance(objects.begin(),selected))});Input::Require(model.properties->GetSelectedObject()==root&&editor.SelectionCount()==1,"Duplicate fixture selected a different authoring object");editor.MenuAction("duplicate");
        Input::Require(Count(model,expected)==2,"Duplicating a recipe root lost its hierarchy icon");
        Input::Require(editor.Undo()&&Count(model,expected)==1,"Undo did not restore the original recipe icon");
        Input::Require(editor.Redo()&&Count(model,expected)==2,"Redo did not restore duplicated recipe icons");
    }
    editor.ApplyDocument(empty);
}
json NestedFixture(){
    auto record=[](int id,int parent,const std::string& name){return json{{"oid",id},{"par",parent},{"base",{{"name",name},{"drag",true},{"coords",{{"x",4},{"y",4},{"width",100},{"height",60}}}}}};};
    json records=json::array();records.push_back(record(0,-1,"Panel"));auto label=record(1,0,"First label");label["text"]="First";records.push_back(label);
    auto bar=record(2,0,"Sibling bar");bar["bar"]={{"value",50},{"min",0},{"max",100}};records.push_back(bar);
    for(int id=3;id<11;++id)records.push_back(record(id,id==3?0:id-1,"Nested "+std::to_string(id)));
    label=record(11,10,"Needle leaf");label["text"]="Deep label";records.push_back(label);
    return {{"canvas",{{"width",320},{"height",240}}},{"obj",records},{"variables",json::array()}};
}
lv_obj_t* Disclosure(TreeNode* node){for(uint32_t i=0;i<lv_obj_get_child_count(node->GetRowObject());++i){auto* child=lv_obj_get_child(node->GetRowObject(),i);if(lv_obj_has_class(child,&lv_button_class))return child;}throw std::runtime_error("Hierarchy disclosure button is missing");}
void NestedInteraction(UI& editor,SdlSimulator& simulator,Model model,const std::filesystem::path& directory){
    editor.ApplyDocument(NestedFixture());auto* window=Input::WindowFromTitle("Hierarchy");Input::Require(window,"Hierarchy panel is missing");
    const int width=lv_obj_get_width(window);lv_obj_set_width(window,160);lv_obj_update_layout(lv_screen_active());
    auto* panel=Named(model.tree,"Panel");auto* leaf=Named(model.tree,"Needle leaf");auto* sibling=Named(model.tree,"Sibling bar");
    for(auto* object:model.tree->GetObjects())CheckRow(model,object,ObjectIcons::ForObject(object));
    auto* node=CheckRow(model,leaf,ObjectIcons::Widget(14));Input::Click(node->GetIconObject(),simulator);Input::Require(model.properties->GetSelectedObject()==leaf,"Clicking a hierarchy icon did not select its object");
    auto* parent=CheckRow(model,panel,ObjectIcons::Widget(8));Input::Click(Disclosure(parent),simulator);Input::Require(Hidden(node->GetIconObject()),"Collapsed hierarchy retained a visible descendant icon");
    Input::Click(Disclosure(parent),simulator);Input::Require(!Hidden(node->GetIconObject()),"Expanding the hierarchy did not restore descendant icons");
    auto* search=Input::FindPlaceholder(window,"Search objects...");Input::Require(search,"Hierarchy filter is missing");Input::Replace(search,"Needle",simulator);
    Input::Require(!Hidden(node->GetIconObject())&&Hidden(model.tree->GetNodeByObject(sibling)->GetIconObject()),"Hierarchy filtering did not follow object icons");
    Input::Click(node->GetIconObject(),simulator);Input::Require(model.properties->GetSelectedObject()==leaf,"Filtered hierarchy icon did not select the leaf");Input::Replace(search,"",simulator);
    model.tree->BeginRename(leaf);auto* field=Input::FindClass(node->GetRowObject(),&lv_textarea_class);Input::Require(field,"Inline hierarchy rename is missing");
    lv_area_t fb{},ib{},rb{};lv_obj_get_coords(field,&fb);lv_obj_get_coords(node->GetIconObject(),&ib);lv_obj_get_coords(node->GetRowObject(),&rb);
    Input::Require(fb.x1>ib.x2&&fb.x2<=rb.x2,"Deep narrow inline rename overlaps its icon or clips outside the row");
    Input::Click(field,simulator);Input::AssertFocused(field,simulator);auto* textLabel=lv_textarea_get_label(field);
    const auto length=uint32_t(std::string(lv_textarea_get_text(field)).size());Input::Require(length>1,"Inline rename selection fixture requires existing text");
    lv_textarea_set_cursor_pos(field,LV_TEXTAREA_CURSOR_LAST);lv_label_set_text_selection_start(textLabel,1);lv_label_set_text_selection_end(textLabel,length);
    Input::Key(SDLK_HOME,SDL_SCANCODE_HOME,simulator);
    Input::Require(lv_textarea_get_cursor_pos(field)==0&&lv_label_get_text_selection_start(textLabel)==LV_LABEL_TEXT_SELECTION_OFF,"Home retained a stale mouse-selected range in the rename field");
    for(uint32_t i=0;i<length;++i)Input::Key(SDLK_DELETE,SDL_SCANCODE_DELETE,simulator);
    Input::Require(std::string(lv_textarea_get_text(field)).empty(),"Home followed by forward Delete left text from an earlier mouse selection");
    Input::Text("Pending draft",simulator);lv_textarea_set_cursor_pos(field,0);lv_label_set_text_selection_start(textLabel,0);lv_label_set_text_selection_end(textLabel,13);
    Input::Key(SDLK_END,SDL_SCANCODE_END,simulator);
    Input::Require(lv_textarea_get_cursor_pos(field)==13&&lv_label_get_text_selection_start(textLabel)==LV_LABEL_TEXT_SELECTION_OFF,"End retained a stale selected range in the rename field");
    Input::Key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE,simulator);Input::Require(std::string(lv_textarea_get_text(field))=="Pending draf","End followed by Backspace deleted the old selection instead of the last character");Input::Text("t",simulator);
    for(int resizedWidth:{420,160}){
        lv_obj_set_width(window,resizedWidth);lv_obj_update_layout(lv_screen_active());
        lv_obj_get_coords(field,&fb);lv_obj_get_coords(node->GetIconObject(),&ib);lv_obj_get_coords(node->GetRowObject(),&rb);
        Input::Require(fb.x1>ib.x2&&fb.x2<=rb.x2&&fb.y1>=rb.y1&&fb.y2<=rb.y2,"Resizing an open inline rename overlaps its icon or clips outside the row");
        Input::Require(std::string(lv_textarea_get_text(field))=="Pending draft","Resizing the hierarchy lost the uncommitted rename text");
    }
    Input::Replace(field,u8"Renamed \u6a21\u578b",simulator);Input::Key(SDLK_RETURN,SDL_SCANCODE_RETURN,simulator);leaf=Named(model.tree,u8"Renamed \u6a21\u578b");CheckRow(model,leaf,ObjectIcons::Widget(14));
    Capture(simulator,directory/"hierarchy-narrow.bmp");
    Input::Require(editor.Undo(),"Inline hierarchy rename was not undoable");CheckRow(model,Named(model.tree,"Needle leaf"),ObjectIcons::Widget(14));
    Input::Require(editor.Redo(),"Inline hierarchy rename was not redoable");CheckRow(model,Named(model.tree,u8"Renamed \u6a21\u578b"),ObjectIcons::Widget(14));
    lv_obj_set_width(window,width);lv_obj_update_layout(lv_screen_active());Capture(simulator,directory/"hierarchy.bmp");
}
}
void CheckHierarchyIcons(UI& editor,SdlSimulator& simulator,const std::filesystem::path& outputDirectory){
    const auto empty=editor.ProjectDocument();const auto directory=outputDirectory.empty()?std::filesystem::temp_directory_path()/("uicreator-hierarchy-icons-"+std::to_string(SDL_GetTicksNS())):std::filesystem::absolute(outputDirectory);
    std::filesystem::create_directories(directory);auto model=CurrentModel();NativeTypes(editor,simulator,model,directory);PresetTypes(editor,model,empty,directory);NestedInteraction(editor,simulator,model,directory);editor.ApplyDocument(empty);
    std::cout<<"Hierarchy icons:35 native types,49 recipe roots, JSON/Protobuf persistence, rename/duplicate/undo, device stripping and24px nested/narrow/filter/physical selection passed\n";
}
