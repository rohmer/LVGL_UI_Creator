#include "EditorInputTest.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Windows/PropertyWindows/PropertyControls.h"
#include "../../../UICreator/Windows/PropertyWindows/BtnmEditorWindow.h"
#include "../../../UICreator/Utils/EditorControls.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Widgets/FileBrowser.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace EditorInputTest;
json Widget(int id,const char* section,json value,int y) {
    return {{"oid",id},{"par",-1},{"base",{{"name",std::string("Input ")+section},{"drag",true},{"coords",{{"x",12},{"y",y},{"width",180},{"height",48}}}}},{section,std::move(value)}};
}
lv_obj_t* Field(lv_obj_t* root,const char* caption,const lv_obj_class_t* type=&lv_textarea_class) {
    auto* field=FindField(root,caption,type);Require(field,std::string("Missing property input: ")+caption);return field;
}
void ChooseNext(lv_obj_t* dropdown,SdlSimulator& simulator) {
    const auto before=lv_dropdown_get_selected(dropdown);
    Click(dropdown,simulator);AssertFocused(dropdown,simulator);
    Require(lv_dropdown_is_open(dropdown),"Clicking a property dropdown immediately closed its popup");
    Key(SDLK_DOWN,SDL_SCANCODE_DOWN,simulator);Key(SDLK_RETURN,SDL_SCANCODE_RETURN,simulator);
    Require(lv_dropdown_get_selected(dropdown)==before+1,"Property dropdown did not accept keyboard selection");
}
void Numeric(PropertyWindow* properties,SdlSimulator& simulator) {
    auto* window=EditorWindow::Create(lv_layer_top());EditorWindow::SetTitle(window,"Numeric entry test");
    lv_obj_set_size(window,420,180);lv_obj_center(window);
    auto* input=lv_spinbox_create(EditorWindow::Content(window));lv_obj_set_size(input,300,42);
    lv_spinbox_set_range(input,-99999,99999);lv_spinbox_set_digit_format(input,5,3);lv_spinbox_set_step(input,1);
    BindEditorControl(properties->GetKBGroup(),input);Click(input,simulator);AssertFocused(input,simulator);
    Text("12.34",simulator);Require(lv_spinbox_get_value(input)==1234,"Typed decimal spinbox text did not update its native value exactly once");
    Key(SDLK_A,SDL_SCANCODE_A,simulator,SDL_KMOD_CTRL);Text("-5.67",simulator);
    Require(lv_spinbox_get_value(input)==-567,"Spinbox Ctrl+A replacement lost its sign or decimal scale");
    Key(SDLK_A,SDL_SCANCODE_A,simulator,SDL_KMOD_CTRL);SDL_SetClipboardText("1000.99");Key(SDLK_V,SDL_SCANCODE_V,simulator,SDL_KMOD_CTRL);
    Require(lv_spinbox_get_value(input)==99999,"Pasted numeric spinbox value did not clamp to its range");
    Key(SDLK_A,SDL_SCANCODE_A,simulator,SDL_KMOD_CTRL);Text("1.25",simulator);lv_spinbox_set_step(input,1);
    Key(SDLK_UP,SDL_SCANCODE_UP,simulator);Require(lv_spinbox_get_value(input)==126,"Spinbox Up lost native step semantics");
    Key(SDLK_LEFT,SDL_SCANCODE_LEFT,simulator);Require(lv_spinbox_get_step(input)==10,"Spinbox Left did not select the next digit");
    Key(SDLK_UP,SDL_SCANCODE_UP,simulator);Require(lv_spinbox_get_value(input)==136,"Spinbox selected-digit increment failed");
    Key(SDLK_RIGHT,SDL_SCANCODE_RIGHT,simulator);Key(SDLK_DOWN,SDL_SCANCODE_DOWN,simulator);
    Require(lv_spinbox_get_value(input)==135,"Spinbox Right/Down lost native digit semantics");
    Key(SDLK_A,SDL_SCANCODE_A,simulator,SDL_KMOD_CTRL);Key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE,simulator);
    Require(lv_spinbox_get_value(input)==0,"Spinbox selected deletion left a stale native value");
    Text("2.50",simulator);Require(lv_spinbox_get_value(input)==250,"Spinbox could not type after selected deletion");
    lv_obj_add_state(input,LV_STATE_DISABLED);Text("9",simulator);Key(SDLK_UP,SDL_SCANCODE_UP,simulator);
    Require(lv_spinbox_get_value(input)==250,"Disabled spinbox accepted keyboard edits");
    lv_obj_remove_state(input,LV_STATE_DISABLED);lv_obj_add_flag(window,LV_OBJ_FLAG_HIDDEN);Text("9",simulator);
    Require(lv_spinbox_get_value(input)==250,"Hidden numeric editor accepted keyboard edits");
    lv_obj_delete(window);Drain(simulator);
}
}

void CheckPropertyEditorInput(UI& editor,SdlSimulator& simulator) {
    using namespace EditorInputTest;
    json fixture={{"formatVersion",2},{"lvglVersion","9.6.0"},{"canvas",{{"width",480},{"height",320}}},
      {"obj",json::array({Widget(0,"text","Original label",12),Widget(1,"button",{{"text","Original button"}},72),
        Widget(2,"btnm",{{"btnMap",json::array({"First"})}},132),Widget(3,"textarea",{{"text","First\nSecond"}},192)})},
      {"variables",json::array({{{"var",{{"name","seed"},{"dataType",1},{"default",7},{"history",1},{"oid",-1},{"property",""}}}}})}};
    editor.ApplyDocument(fixture);editor.SelectObjectIds({0});Drain(simulator);
    auto* propertiesWindow=WindowFromTitle("Properties");Require(propertiesWindow,"Properties window is missing");
    auto* x=Field(propertiesWindow,"X");
    auto* properties=static_cast<sPropChange*>(lv_obj_get_user_data(x))->pw;
    Replace(Field(properties->ObjectPropWin()->GetWindow(),"Text"),u8"Typed label \u03a9",simulator);
    Require(editor.ProjectDocument()["obj"][0]["text"]==u8"Typed label \u03a9","Physical typing did not update the label property");
    Replace(x,"42",simulator);Require(editor.ProjectDocument()["obj"][0]["base"]["coords"]["x"]==42,"Numeric property typing was lost");
    editor.SelectObjectIds({3});Drain(simulator);
    auto* multiline=Field(properties->ObjectPropWin()->GetWindow(),"Text");
    Replace(multiline,"Typed first",simulator);Key(SDLK_RETURN,SDL_SCANCODE_RETURN,simulator);Text("Typed second",simulator);
    Require(editor.ProjectDocument()["obj"][3]["textarea"]["text"]=="Typed first\nTyped second","Multiline property did not accept Enter and continued typing");
    editor.SelectObjectIds({1});Drain(simulator);
    Replace(Field(properties->ObjectPropWin()->GetWindow(),"Text"),"Typed button",simulator);
    Require(editor.ProjectDocument()["obj"][1]["button"]["text"]=="Typed button","Button text typing bypassed the project model");
    ChooseNext(Field(properties->ObjectPropWin()->GetWindow(),"Layout",&lv_dropdown_class),simulator);
    Require(editor.ProjectDocument()["obj"][1]["button"]["layout"]==1,"Button layout keyboard change was not applied");
    ChooseNext(Field(properties->ObjectPropWin()->GetWindow(),"Fit Left",&lv_dropdown_class),simulator);
    Require(editor.ProjectDocument()["obj"][1]["button"]["fit"]["left"]==1,"Button fit keyboard change was not applied");
    auto* global=FindLabel(propertiesWindow,"Global");Require(global,"Global property section is missing");Click(global,simulator);
    auto rollers=Fields(propertiesWindow,&lv_roller_class);Require(rollers.size()==2,"Global property rollers are missing");
    Click(rollers[0],simulator);AssertFocused(rollers[0],simulator);
    Click(rollers[1],simulator);AssertFocused(rollers[1],simulator);const auto hue=lv_roller_get_selected(rollers[1]);
    Key(SDLK_DOWN,SDL_SCANCODE_DOWN,simulator);Require(lv_roller_get_selected(rollers[1])!=hue,"Global hue roller ignored physical keyboard input");Click(global,simulator);

    auto* tools=WindowFromTitle("Tools");Require(tools,"Tools panel is missing");EditorWindow::Show(tools);
    auto* tabs=FindClass(tools,&lv_tabview_class);Require(tabs,"Tools tabs are missing");lv_tabview_set_active(tabs,1,LV_ANIM_OFF);Drain(simulator);
    auto* name=Field(tools,"Name"),*history=Field(tools,"History"),*value=Field(tools,"Default");
    Replace(name,"typedVariable",simulator);Replace(history,"32",simulator);Replace(value,"-125",simulator);
    auto variable=editor.ProjectDocument()["variables"][0]["var"];
    Require(variable["name"]=="typedVariable" && variable["history"]==32 && variable["default"]==-125,"Variable editors lost physical text/numeric edits");
    auto* type=Field(tools,"Type",&lv_dropdown_class);ChooseNext(type,simulator);Replace(value,"true",simulator);
    Require(editor.ProjectDocument()["variables"][0]["var"]["default"]==true,"Variable type/default did not accept keyboard editing");

    auto* hierarchy=properties->GetObjectTree()->GetTreeViewUI();auto* search=FindPlaceholder(hierarchy,"Search objects...");Require(search,"Hierarchy search is missing");
    Replace(search,"button",simulator);Require(std::string(lv_textarea_get_text(search))=="button","Hierarchy search ignored keyboard input");Replace(search,"",simulator);
    editor.SelectObjectIds({0});properties->GetObjectTree()->BeginRename(properties->GetSelectedObject());Drain(simulator);
    lv_obj_t* rename=nullptr;for(auto* entry:Fields(hierarchy))if(entry!=search)rename=entry;
    Require(rename,"Hierarchy rename editor did not open");Replace(rename,"Typed hierarchy name",simulator);Key(SDLK_RETURN,SDL_SCANCODE_RETURN,simulator);
    Require(editor.ProjectDocument()["obj"][0]["base"]["name"]=="Typed hierarchy name","Inline hierarchy rename did not commit typed text");

    editor.SelectObjectIds({2});Drain(simulator);BtnmEditorWindow::CreateBtnmEditorWindow(properties);Drain(simulator);
    auto* matrixWindow=WindowFromTitle("Button Map Editor");Require(matrixWindow,"Legacy button-map editor is missing");
    auto matrixFields=Fields(matrixWindow);Require(matrixFields.size()>=2,"Button-map numeric inputs are missing");
    Replace(matrixFields[0],"220",simulator);
    try {Replace(matrixFields[1],"100",simulator);}
    catch(...) {
        lv_area_t area{};lv_obj_get_coords(matrixFields[1],&area);
        lv_point_t point{(area.x1+area.x2)/2,(area.y1+area.y2)/2};auto topPoint=point;
        auto* hit=lv_indev_search_obj(lv_screen_active(),&point);auto* overlay=lv_indev_search_obj(lv_layer_top(),&topPoint);
        auto* focused=lv_group_get_focused(properties->GetKBGroup());
        std::cerr<<"Button-map Y input="<<matrixFields[1]<<" coords="<<area.x1<<","<<area.y1<<","<<area.x2<<","<<area.y2
                 <<" focus="<<focused<<" screenHit="<<hit<<" overlayHit="<<overlay<<'\n';
        for(auto* object=hit;object;object=lv_obj_get_parent(object)) {
            lv_area_t bounds{};lv_obj_get_coords(object,&bounds);
            std::cerr<<" hit ancestor "<<object<<" textarea="<<lv_obj_has_class(object,&lv_textarea_class)
                     <<" bounds="<<bounds.x1<<","<<bounds.y1<<","<<bounds.x2<<","<<bounds.y2;
            if(lv_obj_has_class(object,&lv_label_class))std::cerr<<" label="<<lv_label_get_text(object);
            std::cerr<<'\n';
        }
        throw;
    }
    auto* preview=FindClass(matrixWindow,&lv_buttonmatrix_class);Require(preview && lv_obj_get_width(preview)==220 && lv_obj_get_height(preview)==100,"Legacy button-map size editors ignored typed numbers");
    Click(EditorWindow::CloseButton(matrixWindow),simulator);

    Numeric(properties,simulator);
    const auto folder=std::filesystem::temp_directory_path()/("uicreator-input-"+std::to_string(SDL_GetTicksNS()));
    const auto nested=folder/std::filesystem::u8path(u8"typed folder \u03a9");std::filesystem::create_directories(nested);
    const auto file=nested/"typed.txt";{std::ofstream stream(file);stream<<"typed";}
    FileBrowser browser(folder.u8string(),lv_layer_top());browser.SetKeyboardGroup(properties->GetKBGroup());Drain(simulator);
    auto* browserWindow=WindowFromTitle("File Browser");auto* path=FindPlaceholder(browserWindow,"Path to a folder or file");Require(path,"File browser path editor is missing");
    Replace(path,nested.u8string(),simulator);Key(SDLK_RETURN,SDL_SCANCODE_RETURN,simulator);
    Require(browser.SelectedPath()==nested,"File browser did not navigate a typed Unicode folder");
    Replace(path,file.u8string(),simulator);Require(browser.SelectedPath()==file,"File browser did not select a typed filename");
    Click(EditorWindow::CloseButton(browserWindow),simulator);std::filesystem::remove_all(folder);
    std::cout<<"Physical editor input passed: properties, legacy controls, variables, hierarchy, numeric entry and file paths\n";
}
