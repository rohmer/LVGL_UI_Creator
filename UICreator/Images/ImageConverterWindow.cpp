#include "ImageConverterWindow.h"
#include "ImageConverter.h"
#ifndef _WIN32
#include "../Utils/NativeFileDialogUI.h"
#endif
#include "../Windows/PropertyWindow.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include <functional>
#include <memory>
#include <thread>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#endif
namespace ImageConverterWindow {
namespace {
struct Job {std::atomic<bool> done{false},cancel{false};ImageConversion::Result result;};
struct Window {PropertyWindow* properties;lv_obj_t *root,*body,*status,*preview,*convert,*apply;std::map<std::string,lv_obj_t*> fields;lv_timer_t* timer=nullptr;std::shared_ptr<Job> job;json embedded=json::array(),assets=json::array();~Window(){if(job)job->cancel=true;if(timer)lv_timer_delete(timer);}};
lv_obj_t* active=nullptr;
void Button(lv_obj_t* parent,const char* text,std::function<void()> action,int width=140) {
    auto* button=lv_button_create(parent);lv_obj_set_size(button,width,36);auto* label=lv_label_create(button);lv_label_set_text(label,text);lv_obj_center(label);
    lv_obj_add_event_cb(button,[](lv_event_t* e){auto* fn=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete fn;else if(lv_event_get_code(e)==LV_EVENT_CLICKED){auto invoke=*fn;invoke();}},LV_EVENT_ALL,new std::function<void()>(std::move(action)));
}
lv_obj_t* Row(lv_obj_t* parent) {auto* row=lv_obj_create(parent);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_style_pad_all(row,0,0);lv_obj_set_style_border_width(row,0,0);lv_obj_set_style_bg_opa(row,0,0);lv_obj_set_style_pad_column(row,10,0);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_scrollable(row,false);lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);return row;}
void Caption(lv_obj_t* parent,const char* text,int width=185){auto* label=lv_label_create(parent);lv_obj_set_width(label,width);lv_label_set_text(label,text);}
void Focus(Window* w,lv_obj_t* field) {BindEditorControl(w->properties->GetKBGroup(),field);}
lv_obj_t* Input(Window* w,const char* label,const std::string& initial,const char* key) {auto* row=Row(w->body);Caption(row,label);auto* field=lv_textarea_create(row);lv_textarea_set_one_line(field,true);lv_textarea_set_text(field,initial.c_str());lv_obj_set_flex_grow(field,1);lv_obj_set_height(field,38);w->fields[key]=field;Focus(w,field);return field;}
void Choice(Window* w,const char* label,const char* choices,uint32_t selected,const char* key) {auto* row=Row(w->body);Caption(row,label);auto* field=lv_dropdown_create(row);lv_obj_set_flex_grow(field,1);lv_dropdown_set_options(field,choices);lv_dropdown_set_selected(field,selected);w->fields[key]=field;Focus(w,field);}
void Check(Window* w,const char* label,bool checked,const char* key) {auto* field=lv_checkbox_create(w->body);lv_checkbox_set_text(field,label);lv_obj_set_checked(field,checked);w->fields[key]=field;}
std::vector<std::filesystem::path> Pick(bool folder) {
    std::vector<std::filesystem::path> paths;
#ifdef _WIN32
    if(folder) {
        auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);IFileOpenDialog* dialog=nullptr;
        if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {DWORD flags=0;dialog->GetOptions(&flags);dialog->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);if(SUCCEEDED(dialog->Show(GetActiveWindow()))){IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR text=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&text))){paths.emplace_back(text);CoTaskMemFree(text);}item->Release();}}dialog->Release();}if(SUCCEEDED(initialized))CoUninitialize();
    } else {
        std::vector<wchar_t> buffer(65536);OPENFILENAMEW dialog{};dialog.lStructSize=sizeof dialog;dialog.hwndOwner=GetActiveWindow();dialog.lpstrFilter=L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.svg\0All files\0*.*\0\0";dialog.lpstrFile=buffer.data();dialog.nMaxFile=static_cast<DWORD>(buffer.size());dialog.Flags=OFN_FILEMUSTEXIST|OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_NOCHANGEDIR;
        if(GetOpenFileNameW(&dialog)){auto first=std::filesystem::path(buffer.data());auto* text=buffer.data()+wcslen(buffer.data())+1;if(!*text)paths.push_back(first);else while(*text){paths.push_back(first/text);text+=wcslen(text)+1;}}
    }
#else
    paths=NativeFileDialogUI::Pick(false,folder,folder?L"Select image or output folder":L"Select source images",L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.svg\0All files\0*.*\0\0",{},!folder);
#endif
    return paths;
}
std::string Text(Window* w,const char* key){return lv_textarea_get_text(w->fields.at(key));}
uint32_t Number(Window* w,const char* key) {const auto text=Text(w,key);size_t end=0;auto value=std::stoul(text,&end,0);if(end!=text.size()||value>UINT32_MAX)throw std::invalid_argument(std::string("Invalid ")+key);return static_cast<uint32_t>(value);}
bool Bool(Window* w,const char* key){return lv_obj_is_checked(w->fields.at(key));}
const std::vector<std::string> colors={"ARGB8888","XRGB8888","RGB888","RGB565","RGB565_SWAPPED","RGB565A8","ARGB8565","ARGB8888_PREMULTIPLIED","L8","AL88","A1","A2","A4","A8","I1","I2","I4","I8","AUTO","RAW","RAW_ALPHA"};
void Start(Window* w) {
    if(w->job&&!w->job->done.load())return;
    try {
        auto options=ImageConversion::Defaults();options["colorFormat"]=colors.at(lv_dropdown_get_selected(w->fields.at("colorFormat")));
        const char* formats[]={"C","BIN","PNG"},*compressions[]={"NONE","RLE","LZ4"};options["format"]=formats[lv_dropdown_get_selected(w->fields.at("format"))];options["compression"]=compressions[lv_dropdown_get_selected(w->fields.at("compression"))];
        options["align"]=Number(w,"align");options["background"]=Number(w,"background");options["chromaColor"]=Number(w,"chromaColor");options["name"]=Text(w,"name");
        for(auto key:{"premultiply","dither","includeAlpha","chromaKey","bigEndian","nemaGfx","verbose"})options[key]=Bool(w,key);
        std::vector<std::filesystem::path> sources;json embedded=json::array();
        if(Bool(w,"embedded"))embedded=w->embedded;else {std::istringstream lines(Text(w,"sources"));std::string line;while(std::getline(lines,line))if(!line.empty())sources.push_back(std::filesystem::u8path(line));}
        if(sources.empty()&&embedded.empty())throw std::invalid_argument("Choose image files or a folder");
        auto output=std::filesystem::u8path(Text(w,"output"));auto job=std::make_shared<Job>();w->job=job;lv_obj_set_disabled(w->convert,true);lv_obj_set_disabled(w->apply,true);lv_label_set_text(w->status,"Converting images...");
        std::thread([job,sources,options,output,embedded]{job->result=ImageConversion::Convert(sources,options,output,&job->cancel,embedded);job->done=true;}).detach();
    }catch(const std::exception& e){lv_label_set_text(w->status,e.what());}
}
void Poll(lv_timer_t* timer) {auto* w=static_cast<Window*>(lv_timer_get_user_data(timer));if(!w->job||!w->job->done.load())return;auto result=std::move(w->job->result);w->job.reset();lv_obj_set_disabled(w->convert,false);if(result.success&&!result.assets.empty()){w->assets=std::move(result.assets);for(const auto& asset:w->assets)w->properties->Images().Add(asset.value("name",std::string("Image")),asset);w->properties->SetSelectedObject(w->properties->GetSelectedObject(),false);try{Serialization::ImageAsset::Restore(w->preview,w->assets[0]);lv_obj_set_size(w->preview,260,160);lv_image_set_inner_align(w->preview,LV_IMAGE_ALIGN_CONTAIN);lv_obj_set_disabled(w->apply,false);}catch(const std::exception& e){result.message+=e.what();}}lv_label_set_text(w->status,result.message.c_str());}
}
void Close() {if(active)lv_obj_delete(active);}
void Open(PropertyWindow* properties,const json& sourceAsset) {
    if(active){lv_obj_move_foreground(active);return;}
    auto* w=new Window{};w->properties=properties;w->root=EditorWindow::Create(lv_layer_top());active=w->root;lv_obj_set_size(w->root,std::min(920,lv_display_get_horizontal_resolution(nullptr)-40),std::min(920,lv_display_get_vertical_resolution(nullptr)-60));lv_obj_center(w->root);EditorWindow::SetTitle(w->root,"Image converter");
    EditorWindow::SetCloseHandler(w->root,[w]{lv_obj_delete(w->root);});
    w->body=EditorWindow::Content(w->root);lv_obj_set_flex_flow(w->body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(w->body,16,0);lv_obj_set_style_pad_row(w->body,10,0);
    Caption(w->body,"LVGL 9.6 image conversion. PNG, JPG, BMP, GIF and SVG; batch files or recursive folders. Original images remain embedded for future conversions.",LV_PCT(100));
    auto* source=Input(w,"Input files / folders","","sources");lv_textarea_set_one_line(source,false);lv_obj_set_height(source,82);
    auto* buttons=Row(w->body);Button(buttons,"Choose files...",[w,source]{auto files=Pick(false);if(files.empty())return;std::string text;for(auto& path:files){if(!text.empty())text+='\n';text+=path.u8string();}lv_textarea_set_text(source,text.c_str());lv_obj_set_checked(w->fields.at("embedded"),false);});
    Button(buttons,"Choose folder...",[w,source]{auto files=Pick(true);if(!files.empty()){lv_textarea_set_text(source,files[0].u8string().c_str());lv_obj_set_checked(w->fields.at("embedded"),false);}});
    if(!sourceAsset.is_null()) {if(!sourceAsset.empty())w->embedded.push_back(sourceAsset);}
    auto* selected=properties->GetSelectedObject();if(sourceAsset.is_null()&&selected&&lv_obj_get_class(selected)==&lv_image_class){auto record=Serialization::ObjectSerializer::SerializeObject(selected);if(record["img"].contains("asset"))w->embedded.push_back(record["img"]["asset"]);}
    Check(w,"Use the selected image's embedded original",!w->embedded.empty(),"embedded");
    std::string choices;for(auto& color:colors){if(!choices.empty())choices+='\n';choices+=color;}Choice(w,"Color format",choices.c_str(),0,"colorFormat");
    Choice(w,"Export format","C source\nLVGL binary\nConverted PNG",0,"format");Input(w,"Output folder (optional)",(std::filesystem::current_path()/"generated_images").u8string(),"output");
    Button(lv_obj_get_parent(w->fields.at("output")),"Browse...",[w]{auto paths=Pick(true);if(!paths.empty())lv_textarea_set_text(w->fields.at("output"),paths[0].u8string().c_str());},110);
    Input(w,"Image symbol (single file)","","name");Choice(w,"Compression","NONE\nRLE\nLZ4",0,"compression");Input(w,"Stride alignment (bytes)","1","align");
    Input(w,"Background RGB","0x000000","background");Check(w,"Preserve alpha / add alpha channel",true,"includeAlpha");Check(w,"Premultiply RGB by alpha",false,"premultiply");Check(w,"Dither indexed / RGB565 pixels",true,"dither");
    Check(w,"Chroma key transparency",false,"chromaKey");Input(w,"Chroma key RGB","0x00ff00","chromaColor");Check(w,"Big-endian RGB565 output",false,"bigEndian");Check(w,"NEMA GFX palette layout (I8)",false,"nemaGfx");Check(w,"Verbose conversion details",false,"verbose");
    Caption(w->body,"RAW formats retain encoded data and require a device decoder; compression and stride apply to decoded formats. Big-endian output uses RGB565_SWAPPED. Export format is a tool output option; project generation still selects C or Protobuf globally.",LV_PCT(100));
    w->preview=lv_image_create(w->body);Serialization::LVObject::FromJSON(json::object(),w->preview);lv_obj_set_size(w->preview,260,160);
    auto* actions=Row(w->body);Button(actions,"Convert images",[w]{Start(w);},180);w->convert=lv_obj_get_child(actions,-1);
    Button(actions,"Apply first image",[w]{try{if(w->assets.empty())throw std::invalid_argument("Convert an image first");auto id=w->properties->Images().Add(w->assets[0].value("name",std::string("Image")),w->assets[0]);w->properties->Images().Apply(w->properties->GetSelectedObject(),id);w->properties->SetSelectedObject(w->properties->GetSelectedObject());lv_label_set_text(w->status,"Converted image and its options are saved with the project.");}catch(const std::exception& e){lv_label_set_text(w->status,e.what());}},200);w->apply=lv_obj_get_child(actions,-1);lv_obj_set_disabled(w->apply,true);
    w->status=lv_label_create(w->body);lv_obj_set_width(w->status,LV_PCT(100));lv_label_set_text(w->status,"Choose input images and conversion options.");
    if(!w->embedded.empty()&&w->embedded[0].contains("conversion")) {
        auto config=ImageConversion::Defaults();config.update(w->embedded[0]["conversion"]);
        for(size_t i=0;i<colors.size();++i)if(config["colorFormat"]==colors[i])lv_dropdown_set_selected(w->fields.at("colorFormat"),i);
        const char* methods[]={"NONE","RLE","LZ4"};for(int i=0;i<3;++i)if(config["compression"]==methods[i])lv_dropdown_set_selected(w->fields.at("compression"),i);
        const char* formats[]={"C","BIN","PNG"};for(int i=0;i<3;++i)if(config["format"]==formats[i])lv_dropdown_set_selected(w->fields.at("format"),i);
        for(auto key:{"premultiply","dither","includeAlpha","chromaKey","bigEndian","nemaGfx","verbose"})lv_obj_set_checked(w->fields.at(key),config.value(key,false));
        for(auto key:{"align","background","chromaColor"})lv_textarea_set_text(w->fields.at(key),config.at(key).dump().c_str());lv_textarea_set_text(w->fields.at("name"),config.value("name",std::string{}).c_str());
    }
    w->timer=lv_timer_create(Poll,100,w);lv_obj_add_event_cb(w->root,[](lv_event_t* e){active=nullptr;delete static_cast<Window*>(lv_event_get_user_data(e));},LV_EVENT_DELETE,w);
}
}
