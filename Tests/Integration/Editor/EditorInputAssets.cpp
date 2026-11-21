#include "EditorInputTest.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>

namespace {
namespace Input=EditorInputTest;

lv_obj_t* OpenAssetDialog(UI& editor,SdlSimulator& simulator,const char* action,const char* title) {
    std::cout<<"Opening asset editor: "<<title<<std::endl;
    editor.MenuAction(action);Input::Drain(simulator);
    auto* window=Input::WindowFromTitle(title);
    Input::Require(window,std::string("Missing asset editor: ")+title);return window;
}

lv_obj_t* TypeAssetField(lv_obj_t* window,const char* caption,const std::string& text,SdlSimulator& simulator) {
    std::cout<<"Typing asset field: "<<caption<<std::endl;
    auto* field=Input::FindField(window,caption);
    Input::Require(field,std::string("Missing editable asset field: ")+caption);
    Input::Replace(field,text,simulator);Input::AssertFocused(field,simulator);
    Input::Require(lv_textarea_get_text(field)==text,std::string("Physical text input did not update asset field: ")+caption);
    return field;
}

void CloseAssetDialog(lv_obj_t* window,SdlSimulator& simulator) {
    std::cout<<"Closing asset editor"<<std::endl;
    auto fields=Input::Fields(window);
    bool deleted=false;
    const auto observe=+[](lv_event_t* event){*static_cast<bool*>(lv_event_get_user_data(event))=true;};
    lv_obj_add_event_cb(window,observe,LV_EVENT_DELETE,&deleted);
    try {Input::Click(EditorWindow::CloseButton(window),simulator);Input::Drain(simulator);}
    catch(...) {if(!deleted)lv_obj_remove_event_cb_with_user_data(window,observe,&deleted);throw;}
    if(!deleted)lv_obj_remove_event_cb_with_user_data(window,observe,&deleted);
    Input::Require(deleted,"Closing an asset editor left its window alive");
    auto* group=lv_indev_get_group(simulator.GetKeyboard());auto* focused=group?lv_group_get_focused(group):nullptr;
    Input::Require(std::find(fields.begin(),fields.end(),focused)==fields.end(),"A destroyed asset-editor field retained physical keyboard focus");
    std::cout<<"Asset editor closed"<<std::endl;
}

lv_obj_t* LastFontSource(lv_obj_t* window) {
    lv_obj_t* field=nullptr;
    std::function<void(lv_obj_t*)> visit=[&](lv_obj_t* object){
        if(lv_obj_has_class(object,&lv_label_class)&&std::string(lv_label_get_text(object))=="Font file")
            if(auto* candidate=Input::FindClass(lv_obj_get_parent(object),&lv_textarea_class))field=candidate;
        for(uint32_t index=0;index<lv_obj_get_child_count(object);++index)visit(lv_obj_get_child(object,index));
    };
    visit(window);return field;
}
}

void CheckAssetEditorInput(UI& editor,SdlSimulator& simulator) {
    const auto original=editor.ProjectDocument();
    auto* window=OpenAssetDialog(editor,simulator,"font-converter","Font converter");
    TypeAssetField(window,"Font file",u8"C:\\Fonts\\typed sample \u00e9.ttf",simulator);
    TypeAssetField(window,"Unicode ranges","0x20-0x7E,0xE000",simulator);
    TypeAssetField(window,"Additional symbols",u8"AZ\u00e9",simulator);
    TypeAssetField(window,"Font symbol","typed_font_32",simulator);
    TypeAssetField(window,"Output path","C:\\output fonts\\typed_font.c",simulator);
    TypeAssetField(window,"Size (pixels)","32",simulator);
    TypeAssetField(window,"LVGL include path","my_lvgl/lvgl.h",simulator);
    TypeAssetField(window,"Fallback font symbol","lv_font_montserrat_14",simulator);
    TypeAssetField(window,"Project root (optional)","C:\\typed project",simulator);
    TypeAssetField(window,"Preview text",u8"Typed preview \u00e9 123",simulator);
    const auto initialFields=Input::Fields(window).size();
    std::cout<<"Adding and typing dynamic font source"<<std::endl;
    Input::Click(Input::ParentButton(Input::FindLabel(window,"Add font source")),simulator);
    Input::Require(Input::Fields(window).size()==initialFields+3,"Adding a font source did not create its three editable fields");
    auto* added=LastFontSource(window);Input::Require(added,"The dynamic font source path is missing");
    Input::Replace(added,"C:\\Fonts\\second typed font.woff",simulator);Input::AssertFocused(added,simulator);
    Input::Require(std::string(lv_textarea_get_text(added))=="C:\\Fonts\\second typed font.woff","Dynamically added font controls did not accept physical typing");
    auto* addedPanel=lv_obj_get_parent(lv_obj_get_parent(added));
    std::cout<<"Removing dynamic font source"<<std::endl;
    Input::Click(Input::ParentButton(Input::FindLabel(addedPanel,"Remove")),simulator);
    Input::Require(Input::Fields(window).size()==initialFields,"Removing a focused font source left its editable fields alive");
    TypeAssetField(window,"Font symbol","typed_after_source_removal",simulator);
    CloseAssetDialog(window,simulator);

    window=OpenAssetDialog(editor,simulator,"font-manager","Project fonts");
    auto* fontList=Input::FindClass(window,&lv_dropdown_class);Input::Click(fontList,simulator);Input::AssertFocused(fontList,simulator);lv_dropdown_close(fontList);
    TypeAssetField(window,"Font name",u8"Typed project font \u00e9",simulator);CloseAssetDialog(window,simulator);

    window=OpenAssetDialog(editor,simulator,"image-converter","Image converter");
    TypeAssetField(window,"Input files / folders","C:\\images\\one.png\nC:\\images\\two.svg",simulator);
    TypeAssetField(window,"Output folder (optional)","C:\\typed image output",simulator);
    TypeAssetField(window,"Image symbol (single file)","typed_image",simulator);
    TypeAssetField(window,"Stride alignment (bytes)","16",simulator);
    TypeAssetField(window,"Background RGB","0x114477",simulator);
    TypeAssetField(window,"Chroma key RGB","0xff00ff",simulator);CloseAssetDialog(window,simulator);

    window=OpenAssetDialog(editor,simulator,"image-manager","Project images");
    auto* imageList=Input::FindClass(window,&lv_dropdown_class);Input::Click(imageList,simulator);Input::AssertFocused(imageList,simulator);lv_dropdown_close(imageList);
    TypeAssetField(window,"Image name",u8"Typed project image \u00e9",simulator);CloseAssetDialog(window,simulator);

    window=OpenAssetDialog(editor,simulator,"project-creator","Create LVGL firmware project");
    TypeAssetField(window,"Search templates","raspberry",simulator);
    TypeAssetField(window,"Search templates","",simulator);
    auto* lvglFolder=Input::FindField(window,"LVGL folder (auto)");
    Input::Require(lvglFolder&&!lv_obj_has_state(lvglFolder,LV_STATE_DISABLED),"The default generic project template unexpectedly disabled its editable LVGL path");
    TypeAssetField(window,"LVGL folder (auto)","C:\\typed SDK\\lvgl",simulator);
    TypeAssetField(window,"Destination folder",u8"C:\\projects\\typed device \u00e9",simulator);CloseAssetDialog(window,simulator);
    Input::Require(editor.ProjectDocument()==original,"Typing in asset/project dialogs altered the authored canvas");
}
