#include "ComponentLibrary.h"
#ifndef _WIN32
#include "../Utils/NativeFileDialogUI.h"
#endif
#include "ComponentReview.h"
#include "../ProjectThemes/AuthoringControls.h"
#include "../PreviewProfiles/PreviewSurface.h"
#include "../PreviewProfiles/PreviewDialog.h"
#include <filesystem>
#include <fstream>
#include <cctype>
#include <set>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace ComponentLibraryWindow {
namespace {
using namespace ProjectAuthoring;
namespace Design=Serialization::ProjectDesign;
struct Window {DesignContext context;lv_obj_t *root=nullptr,*choice=nullptr,*name=nullptr,*description=nullptr,*status=nullptr,*search=nullptr,*previewHost=nullptr;std::vector<std::string> ids;
    std::unique_ptr<PreviewProfiles::PreviewSurface> thumbnail;std::unique_ptr<PreviewProfiles::PreviewDialog> confirmation;
    ~Window(){confirmation.reset();thumbnail.reset();}
};
Window* active=nullptr;
std::filesystem::path Pick(bool save)
{
#ifdef _WIN32
    wchar_t path[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof dialog;dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=path;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"LVGL component library (*.json)\0*.json\0All files\0*.*\0\0";dialog.lpstrDefExt=L"json";
    dialog.lpstrTitle=save?L"Export component library":L"Import component library";
    dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))return path;
#else
    return NativeFileDialogUI::PickOne(save,false,save?L"Export component library":L"Import component library",L"LVGL component library\0*.json\0All files\0*.*\0\0",save?std::filesystem::path("components.json"):std::filesystem::path{});
#endif
    return {};
}
std::string Selected(Window* w)
{
    const unsigned index=lv_dropdown_get_selected(w->choice);if(index>=w->ids.size())throw std::invalid_argument("Capture or import a component first");return w->ids[index];
}
void Details(Window* w)
{
    if(w->ids.empty()){lv_label_set_text(w->description,"No matching components. Clear the search or capture a widget or parent container from the canvas.");if(w->thumbnail)w->thumbnail->Clear();return;}
    auto doc=w->context.document();const auto id=Selected(w);const auto& def=Design::Definition(doc,id);
    lv_textarea_set_text(w->name,def.value("name",id).c_str());
    lv_label_set_text_fmt(w->description,"%u widgets   |   %u linked instances   |   revision %d",static_cast<unsigned>(def.at("records").size()),static_cast<unsigned>(Design::InstanceCount(doc,id)),def.value("revision",1));
    try{
        auto preview=doc;preview.erase("screens");preview.erase("activeScreen");preview.erase("editorScreen");preview["obj"]=def.at("records");preview["variables"]=json::array();
        int width=240,height=160;std::set<int> ids;for(const auto& record:preview["obj"])ids.insert(record.value("oid",-1));
        for(auto& record:preview["obj"]){record["base"].erase("events");if(record.contains("advancedKeyboard")&&!ids.count(record["advancedKeyboard"].value("targetOid",-1)))record["advancedKeyboard"]["targetOid"]=-1;
            if(record.value("par",-1)<0){const auto coords=record.value("base",json::object()).value("coords",json::object());width=std::max(width,coords.value("x",0)+coords.value("width",240));height=std::max(height,coords.value("y",0)+coords.value("height",160));}}
        preview["canvas"]={{"width",(std::min)(width,2048)},{"height",(std::min)(height,2048)}};
        lv_obj_update_layout(w->root);if(!w->thumbnail)w->thumbnail=std::make_unique<PreviewProfiles::PreviewSurface>(w->previewHost);
        w->thumbnail->Load(std::move(preview),(std::min)(width,2048),(std::min)(height,2048),w->context.sourceDirectory);w->thumbnail->Freeze();
    }catch(const std::exception& e){if(w->status)lv_label_set_text(w->status,e.what());}
}
void Refresh(Window* w,const std::string& chosen={})
{
    auto doc=w->context.document();w->ids.clear();std::string labels;unsigned selected=0;
    if(doc.contains("components")&&doc["components"].is_array())for(const auto& def:doc["components"]) {
        if(!def.is_object()||!def.contains("id"))continue;
        auto lower=[](std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;};
        const std::string query=w->search?lv_textarea_get_text(w->search):"";if(!query.empty()&&lower(def.value("name",std::string())).find(lower(query))==std::string::npos)continue;
        const auto id=def.at("id").get<std::string>();if(id==chosen)selected=static_cast<unsigned>(w->ids.size());w->ids.push_back(id);
        auto name=def.value("name",id);for(auto& c:name)if(c=='\r'||c=='\n')c=' ';if(!labels.empty())labels+='\n';labels+=name;
    }
    lv_dropdown_set_options(w->choice,labels.empty()?"No saved components":labels.c_str());lv_dropdown_set_selected(w->choice,selected);Details(w);
}
template<class Action> void Edit(Window* w,Action action,const char* status)
{
    try {
        const auto context=w->context;auto doc=context.document();const auto selected=context.selection();
        const auto id=action(doc,selected);context.apply(std::move(doc));
        if(active==w){Refresh(w,id);lv_label_set_text(w->status,status);}
    }catch(const std::exception& e){if(active==w)lv_label_set_text(w->status,e.what());}
}
}
void Close(){if(active){auto* root=active->root;EditorWindow::ReleaseFocus(root);lv_obj_delete(root);}}
void Open(ProjectAuthoring::DesignContext context)
{
    Close();auto* w=new Window;w->context=std::move(context);active=w;
    w->root=EditorWindow::Create(lv_layer_top());lv_obj_set_size(w->root,850,820);lv_obj_set_style_max_width(w->root,LV_PCT(94),0);lv_obj_set_style_max_height(w->root,LV_PCT(94),0);lv_obj_center(w->root);EditorWindow::SetTitle(w->root,"Component library");
    lv_obj_add_event_cb(w->root,[](lv_event_t* e){auto* state=static_cast<Window*>(lv_event_get_user_data(e));if(active==state)active=nullptr;delete state;},LV_EVENT_DELETE,w);
    EditorWindow::SetCloseHandler(w->root,[]{Close();});
    auto* body=EditorWindow::Content(w->root);Column(body);
    EditorStyle::Help(body,"Build reusable groups from ordinary LVGL widgets. Insert linked instances, edit one on the canvas, then update its master to share the change. Local text and style overrides on other instances are preserved.");
    w->choice=lv_dropdown_create(body);lv_obj_set_width(w->choice,LV_PCT(100));EditorStyle::Control(w->choice);Focus(w->choice,w->context.keyboardGroup);
    w->description=EditorStyle::Help(body,"");w->name=Field(body,"Component name",w->context.keyboardGroup);lv_textarea_set_max_length(w->name,120);
    w->search=Field(body,"Search saved components",w->context.keyboardGroup);lv_obj_add_event_cb(w->search,[](lv_event_t* e){Refresh(static_cast<Window*>(lv_event_get_user_data(e)));},LV_EVENT_VALUE_CHANGED,w);
    w->previewHost=lv_obj_create(body);lv_obj_set_size(w->previewHost,LV_PCT(100),175);lv_obj_set_scrollable(w->previewHost,false);lv_obj_set_style_pad_all(w->previewHost,0,0);lv_obj_set_style_bg_color(w->previewHost,lv_color_hex(0x111820),0);
    lv_obj_add_event_cb(w->choice,[](lv_event_t* e){Details(static_cast<Window*>(lv_event_get_user_data(e)));},LV_EVENT_VALUE_CHANGED,w);
    auto* row=Row(body);
    Button(row,"Capture selection",[w]{const std::string name=lv_textarea_get_text(w->name);Edit(w,[name](json& doc,const std::vector<int>& selected){return Design::Capture(doc,selected,name);},"Component saved. Insert it to create a linked instance.");},190,w->context.keyboardGroup);
    Button(row,"Insert linked instance",[w]{
        try {const auto id=Selected(w);const auto context=w->context;auto doc=context.document();const auto roots=Design::Insert(doc,id);context.apply(std::move(doc));if(context.select)context.select(roots);if(active==w){Refresh(w,id);lv_label_set_text(w->status,"Linked instance inserted. Move it on the canvas and edit its contents.");}}
        catch(const std::exception& e){if(active==w)lv_label_set_text(w->status,e.what());}
    },210,w->context.keyboardGroup);
    row=Row(body);
    Button(row,"Update selected master",[w]{try{const auto before=w->context.document();auto review=ReviewMaster(before,w->context.selection());w->confirmation=std::make_unique<PreviewProfiles::PreviewDialog>(w->root,w->context,review.candidate,"Review component master update",review.summary,"Apply master update",[w,before,candidate=review.candidate]{if(w->context.document()!=before){lv_label_set_text(w->status,"The project changed. Review the master update again.");w->confirmation.reset();return;}auto context=w->context;context.apply(candidate);if(active==w){w->confirmation.reset();Refresh(w);lv_label_set_text(w->status,"Master updated across linked instances; local overrides retained. Undo is available.");}},[w]{w->confirmation.reset();});}catch(const std::exception& e){lv_label_set_text(w->status,e.what());}},220,w->context.keyboardGroup);
    Button(row,"Detach selected instance",[w]{Edit(w,[](json& doc,const std::vector<int>& selected){Design::Detach(doc,selected);return std::string();},"Instance detached. Its widgets remain fully editable.");},230,w->context.keyboardGroup);
    row=Row(body);
    Button(row,"Rename definition",[w]{try{const auto id=Selected(w);const std::string name=lv_textarea_get_text(w->name);Edit(w,[id,name](json& doc,const std::vector<int>&){Design::Rename(doc,id,name);return id;},"Component renamed.");}catch(const std::exception& e){lv_label_set_text(w->status,e.what());}},190,w->context.keyboardGroup);
    Button(row,"Delete unused",[w]{try{const auto id=Selected(w);Edit(w,[id](json& doc,const std::vector<int>&){Design::RemoveUnused(doc,id);return std::string();},"Unused definition removed. Undo is available.");}catch(const std::exception& e){lv_label_set_text(w->status,e.what());}},180,w->context.keyboardGroup);
    EditorStyle::Help(body,"Capture includes descendants of the selected widgets. Master updates include layout and new children. Enter a name above when saving a new component. All changes support Undo.");
    row=Row(body);
    Button(row,"Import library...",[w]{try{const auto path=Pick(false);if(path.empty())return;if(std::filesystem::file_size(path)>64u*1024u*1024u)throw std::invalid_argument("Component library exceeds 64 MB");std::ifstream file(path);if(!file)throw std::runtime_error("Cannot open component library");json imported;file>>imported;Edit(w,[imported](json& doc,const std::vector<int>&){Design::ImportLibrary(doc,imported);return std::string();},"Component library imported with its project assets.");}catch(const std::exception& e){if(active==w)lv_label_set_text(w->status,e.what());}},180,w->context.keyboardGroup);
    Button(row,"Export library...",[w]{try{const auto path=Pick(true);if(path.empty())return;const auto library=Design::ExportLibrary(w->context.document());std::ofstream file(path,std::ios::binary);file<<library.dump(2);if(!file)throw std::runtime_error("Cannot write component library");lv_label_set_text(w->status,"Library exported, including referenced project assets.");}catch(const std::exception& e){if(active==w)lv_label_set_text(w->status,e.what());}},180,w->context.keyboardGroup);
    w->status=EditorStyle::Help(body,"Components remain native LVGL widget records on Windows, ESP32, Linux and Arduino.");Refresh(w);
}
}
