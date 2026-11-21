#include "CodeWidgets.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include <Serialization/CodeWidgets.h>
#include <iostream>
#include <stdexcept>

namespace
{
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
lv_obj_t* Label(lv_obj_t* root, const char* text)
{
    if (lv_obj_get_class(root) == &lv_label_class && std::string(lv_label_get_text(root)) == text) return root;
    for (uint32_t i=0;i<lv_obj_get_child_count(root);++i) if (auto* result=Label(lv_obj_get_child(root,i),text)) return result;
    return nullptr;
}
lv_obj_t* Field(PropertyWindow* properties, const char* caption)
{
    auto* label=Label(properties->ObjectPropWin()->GetWindow(),caption);
    if(!label)throw std::runtime_error(std::string("Code inspector field missing: ")+caption);
    auto* field=lv_obj_get_child(lv_obj_get_parent(label),1);Require(field,"Code inspector input missing");return field;
}
void SetText(lv_obj_t* field, const char* value)
{
    lv_textarea_set_text(field,value);lv_obj_send_event(field,LV_EVENT_VALUE_CHANGED,nullptr);
}
}
void CheckCodeWidgetEditor(UI& editor, SdlSimulator& simulator)
{
    auto* trayTitle=Label(lv_screen_active(),"Tool Tray");Require(trayTitle,"Tool Tray missing");
    auto* palette=EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(trayTitle)));
    auto click=[&](const char* text) {
        auto* label=Label(palette,text);Require(label,"Advanced code palette entry missing");
        lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);lv_obj_update_layout(lv_screen_active());
    };
    click("Advanced\nWidgets");click("Barcode");
    auto* simulatorTitle=Label(lv_screen_active(),"Simulator [Size:320x240]");Require(simulatorTitle,"Simulator missing");
    auto* surface=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(simulatorTitle))),0);
    auto* barcode=lv_obj_get_child(surface,0);Require(lv_obj_get_class(barcode)==&lv_barcode_class,"Palette did not create a native barcode");
    auto* properties=static_cast<ObjectUserData*>(lv_obj_get_user_data(barcode))->toolTray->GetPropertyWindow();
    SetText(Field(properties,"Data"),"00001111");
    Require(editor.ProjectDocument()["obj"][0]["barcode"]["data"]=="00001111" && lv_barcode_is_render_valid(barcode),"Barcode inspector did not apply content");
    Require(editor.Undo(),"Barcode content edit was not undoable");
    Require(editor.ProjectDocument()["obj"][0]["barcode"]["data"]=="12345678","Barcode undo lost its original content");
    Require(editor.Redo(),"Barcode content edit was not redoable");
    barcode=lv_obj_get_child(surface,0);
    Require(editor.ProjectDocument()["obj"][0]["barcode"]["data"]=="00001111" && lv_barcode_is_render_valid(barcode),"Barcode redo lost its content");
    auto* encoding=Field(properties,"Encoding");lv_dropdown_set_selected(encoding,0);lv_obj_send_event(encoding,LV_EVENT_VALUE_CHANGED,nullptr);
    Require(editor.ProjectDocument()["obj"][0]["barcode"]["encoding"]==0,"Barcode encoding selector did not apply GS1");
    const auto valid=editor.ProjectDocument();
    SetText(Field(properties,"Scale"),"0");Require(editor.ProjectDocument()==valid && lv_barcode_is_render_valid(barcode),"Invalid barcode scale changed the design");
    editor.MenuAction("select-screen");click("QR code");
    auto* qr=lv_obj_get_child(surface,1);Require(qr && lv_obj_get_class(qr)==&lv_qrcode_class,"Palette did not create a native QR code");
    SetText(Field(properties,"Data"),"ESP32 device 123");SetText(Field(properties,"Size"),"192");
    Require(lv_obj_get_width(qr)==192 && lv_obj_get_height(qr)==192 && lv_qrcode_is_render_valid(qr),"QR inspector size/content did not render");
    auto project=editor.ProjectDocument();
    Require(project["obj"].size()==2 && project["obj"][1]["qrcode"]["data"]=="ESP32 device 123","QR properties were not serialized");
    Require(!project["obj"][0].contains("canvas") && !project["obj"][1].contains("canvas"),"Code widgets serialized generated canvas pixels");
    editor.MenuAction("export-c");editor.MenuAction("export-preflight");
    bool ready=false;
    auto findReady=[&](auto&& self,lv_obj_t* object)->void {
        if(lv_obj_get_class(object)==&lv_label_class && std::string(lv_label_get_text(object)).find("Ready for LVGL 9.6")==0)ready=true;
        for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)self(self,lv_obj_get_child(object,i));
    };
    findReady(findReady,lv_layer_top());Require(ready,"Code widgets did not pass C export preflight");editor.MenuAction("cancel-file");
    for(int frame=0;frame<3;++frame)simulator.ProcessEvents();
    std::cout<<"Advanced barcode/QR palette, inspector, validation, undo/redo and export preflight passed\n";
}
