#include "FontConverterWindow.h"
#include "FontConverter.h"
#include "BuiltinFonts.h"
#ifndef _WIN32
#include "../Utils/NativeFileDialogUI.h"
#endif
#include "../Windows/PropertyWindow.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include <Serialization/LVFont.h>
#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace FontConverterWindow {
namespace {
enum class Action { Preview, Save, Apply };
struct Job {
    std::atomic<bool> done{false},cancel{false};
    FontConversion::Result result;
    std::string name;
    json settings;
    Action action=Action::Preview;
};
struct SourceRow {lv_obj_t *root,*catalog,*path,*ranges,*symbols,*hint;};
struct Window {
    PropertyWindow* properties;
    lv_obj_t *root=nullptr,*sources=nullptr,*status=nullptr,*preview=nullptr,*sample=nullptr;
    lv_obj_t *previewButton=nullptr,*saveButton=nullptr,*applyButton=nullptr,*target=nullptr;
    std::map<std::string,lv_obj_t*> fields;
    std::vector<SourceRow> rows;
    lv_timer_t* timer=nullptr;
    lv_font_t* previewFont=nullptr;
    std::shared_ptr<Job> job;
    std::string projectFont,initialBuiltin;
    ~Window();
};
lv_obj_t* active=nullptr;
Window* activeWindow=nullptr;
void TargetDeleted(lv_event_t* event) {static_cast<Window*>(lv_event_get_user_data(event))->target=nullptr;}
void Target(Window* w,lv_obj_t* object) {
    if(w->target)lv_obj_remove_event_cb_with_user_data(w->target,TargetDeleted,w);
    w->target=object;
    if(object)lv_obj_add_event_cb(object,TargetDeleted,LV_EVENT_DELETE,w);
}
Window::~Window() {
    if(job)job->cancel=true;
    if(timer)lv_timer_delete(timer);
    Target(this,nullptr);
    if(previewFont){lv_obj_set_style_text_font(preview,LV_FONT_DEFAULT,0);lv_binfont_destroy(previewFont);}
}
lv_obj_t* Button(lv_obj_t* parent,const char* label,std::function<void()> action,int width=130) {
    auto* button=lv_button_create(parent);lv_obj_set_size(button,width,36);lv_obj_set_style_shadow_width(button,0,0);
    auto* text=lv_label_create(button);lv_label_set_text(text,label);lv_obj_center(text);
    auto* callback=new std::function<void()>(std::move(action));
    lv_obj_add_event_cb(button,[](lv_event_t* event){auto* fn=static_cast<std::function<void()>*>(lv_event_get_user_data(event));if(lv_event_get_code(event)==LV_EVENT_DELETE){delete fn;return;}if(lv_event_get_code(event)==LV_EVENT_CLICKED){auto action=*fn;action();}},LV_EVENT_ALL,callback);
    return button;
}
lv_obj_t* Row(lv_obj_t* parent) {
    auto* row=lv_obj_create(parent);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_style_pad_all(row,0,0);lv_obj_set_style_border_width(row,0,0);
    lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);lv_obj_set_style_pad_column(row,10,0);lv_obj_set_scrollable(row,false);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);return row;
}
void Label(lv_obj_t* parent,const char* text,int width=175) {auto* label=lv_label_create(parent);lv_label_set_text(label,text);lv_obj_set_width(label,width);lv_label_set_long_mode(label,LV_LABEL_LONG_MODE_WRAP);}
void Focus(Window* w,lv_obj_t* control) {BindEditorControl(w->properties->GetKBGroup(),control);}
lv_obj_t* Input(Window* w,lv_obj_t* parent,const char* caption,const std::string& initial,const std::string& key="") {
    auto* row=Row(parent);Label(row,caption);
    auto* field=lv_textarea_create(row);lv_textarea_set_one_line(field,true);lv_textarea_set_text(field,initial.c_str());lv_obj_set_height(field,38);lv_obj_set_flex_grow(field,1);Focus(w,field);
    if(!key.empty())w->fields[key]=field;return field;
}
lv_obj_t* Choice(Window* w,lv_obj_t* parent,const char* caption,const char* choices,int selected,const char* key) {
    auto* row=Row(parent);Label(row,caption);auto* field=lv_dropdown_create(row);lv_obj_set_flex_grow(field,1);lv_dropdown_set_options(field,choices);lv_dropdown_set_selected(field,selected);Focus(w,field);if(key&&*key)w->fields[key]=field;return field;
}
void Check(Window* w,lv_obj_t* parent,const char* caption,bool selected,const char* key) {
    auto* field=lv_checkbox_create(parent);lv_checkbox_set_text(field,caption);lv_obj_set_checked(field,selected);w->fields[key]=field;Focus(w,field);
}
void Section(lv_obj_t* parent,const char* title) {auto* label=lv_label_create(parent);lv_label_set_text(label,title);lv_obj_set_style_text_color(label,lv_color_hex(0x7bbcff),0);lv_obj_set_style_pad_top(label,8,0);}
std::filesystem::path PickFont() {
#ifdef _WIN32
    wchar_t path[32768]={};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof dialog;dialog.hwndOwner=GetActiveWindow();
    dialog.lpstrFilter=L"Fonts (*.ttf;*.otf;*.woff;*.woff2)\0*.ttf;*.otf;*.woff;*.woff2\0All files\0*.*\0\0";
    dialog.lpstrTitle=L"Select source font";dialog.lpstrFile=path;dialog.nMaxFile=_countof(path);
    dialog.Flags=OFN_NOCHANGEDIR|OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    if(GetOpenFileNameW(&dialog))return std::filesystem::path(path);
#else
    return NativeFileDialogUI::PickOne(false,false,L"Select source font",L"Fonts (*.ttf;*.otf;*.woff;*.woff2)\0*.ttf;*.otf;*.woff;*.woff2\0All files\0*.*\0\0",{});
#endif
    return {};
}
std::string Text(Window* w,const char* key) {return lv_textarea_get_text(w->fields.at(key));}
int Integer(Window* w,const char* key) {const auto text=Text(w,key);size_t end=0;int value=std::stoi(text,&end);if(end!=text.size())throw std::invalid_argument(std::string("Invalid ")+key);return value;}
bool Checked(Window* w,const char* key) {return lv_obj_is_checked(w->fields.at(key));}
void AddSource(Window* w,const json& saved=json::object()) {
    auto* panel=lv_obj_create(w->sources);lv_obj_set_size(panel,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(panel,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(panel,10,0);lv_obj_set_style_pad_row(panel,8,0);lv_obj_set_scrollable(panel,false);
    const auto* builtin=BuiltinFonts::Find(saved.value("builtinId",std::string{}));
    std::string choices="Choose a file...";uint32_t selected=0,index=1;
    for(const auto& entry:BuiltinFonts::Catalog()){choices+='\n';choices+=entry.name;if(builtin&&entry.id==builtin->id)selected=index;++index;}
    SourceRow source{};source.root=panel;source.catalog=Choice(w,panel,"Source font",choices.c_str(),static_cast<int>(selected),"");
    source.path=Input(w,panel,"Font file",builtin?builtin->path.u8string():saved.value("path",std::string{}));
    lv_obj_set_hidden(lv_obj_get_parent(source.path),builtin!=nullptr);
    auto* path=source.path;
    Button(lv_obj_get_parent(path),"Browse...",[w,path] {auto selected=PickFont();if(selected.empty())return;lv_textarea_set_text(path,selected.u8string().c_str());},110);
    source.ranges=Input(w,panel,"Unicode ranges",saved.value("ranges",std::string("0x20-0x7E")));lv_textarea_set_placeholder_text(source.ranges,"0x20-0x7E,0x400-0x45F or 0xE000=>0xF000");
    source.symbols=Input(w,panel,"Additional symbols",saved.value("symbols",std::string{}));
    source.hint=Choice(w,panel,"Hinting","Light (default)\nOff\nStrong",std::clamp(saved.value("hinting",0),0,2),"");
    Button(lv_obj_get_parent(source.hint),"Remove",[w,panel] {auto it=std::find_if(w->rows.begin(),w->rows.end(),[panel](const SourceRow& r){return r.root==panel;});if(it!=w->rows.end())w->rows.erase(it);lv_obj_delete(panel);},110);
    w->rows.push_back(source);
    lv_obj_add_event_cb(source.catalog,[](lv_event_t* event){
        auto* w=static_cast<Window*>(lv_event_get_user_data(event));auto* choice=lv_event_get_target_obj(event);
        auto row=std::find_if(w->rows.begin(),w->rows.end(),[choice](const SourceRow& source){return source.catalog==choice;});if(row==w->rows.end())return;
        const auto selected=lv_dropdown_get_selected(choice);const auto& catalog=BuiltinFonts::Catalog();
        const auto* entry=selected&&selected<=catalog.size()?&catalog[selected-1]:nullptr;
        if(entry)lv_textarea_set_text(row->path,entry->path.u8string().c_str());else lv_textarea_set_text(row->path,"");
        lv_obj_set_hidden(lv_obj_get_parent(row->path),entry!=nullptr);
        if(row==w->rows.begin())w->initialBuiltin=entry?entry->id:std::string{};
    },LV_EVENT_VALUE_CHANGED,w);
}
FontConversion::Options ReadOptions(Window* w) {
    FontConversion::Options o;
    for(const auto& row:w->rows)o.sources.push_back({std::filesystem::u8path(lv_textarea_get_text(row.path)),lv_textarea_get_text(row.ranges),lv_textarea_get_text(row.symbols),int(lv_dropdown_get_selected(row.hint))});
    o.size=Integer(w,"size");const int bpp[]={1,2,3,4,8};o.bpp=bpp[lv_dropdown_get_selected(w->fields.at("bpp"))];
    o.smoothing=lv_dropdown_get_selected(w->fields.at("smoothing"));o.compress=Checked(w,"compress");o.prefilter=true;o.kerning=Checked(w,"kerning");o.colorInfo=Checked(w,"colorInfo");
    return o;
}
json Settings(const FontConversion::Options& o,Window* w) {
    auto sources=json::array();for(size_t i=0;i<o.sources.size();++i){const auto& source=o.sources[i];json settings={{"ranges",source.ranges},{"symbols",source.symbols},{"hinting",source.hinting}};
        const auto selected=lv_dropdown_get_selected(w->rows[i].catalog);const auto& catalog=BuiltinFonts::Catalog();
        if(selected&&selected<=catalog.size())settings["builtinId"]=catalog[selected-1].id;else settings["path"]=source.path.u8string();sources.push_back(std::move(settings));}
    return {{"sources",std::move(sources)},{"size",o.size},{"bpp",o.bpp},{"smoothing",o.smoothing},{"compress",o.compress},{"kerning",o.kerning},{"colorInfo",o.colorInfo},{"sample",lv_textarea_get_text(w->sample)}};
}
void Actions(Window* w,bool busy) {
    lv_obj_set_disabled(w->previewButton,busy);lv_obj_set_disabled(w->saveButton,busy);lv_obj_set_disabled(w->applyButton,busy);
    lv_label_set_text(lv_obj_get_child(w->saveButton,0),w->projectFont.empty()?"Add to project":"Save font");
    lv_label_set_text(lv_obj_get_child(w->applyButton,0),w->projectFont.empty()?"Add and apply":"Save and apply");
}
void Start(Window* w,Action action) {
    if(w->job&&!w->job->done)return;
    try {
        auto options=ReadOptions(w);
        if(options.sources.empty())throw std::invalid_argument("Choose at least one source font.");
        if(action==Action::Apply&&!w->properties->GetSelectedObject())throw std::invalid_argument("Select the widget that should use this font.");
        auto name=Text(w,"name");const auto first=name.find_first_not_of(" \t\r\n");name=first==std::string::npos?std::string{}:name.substr(first,name.find_last_not_of(" \t\r\n")-first+1);
        if(name.empty()){const auto selected=lv_dropdown_get_selected(w->rows.front().catalog);const auto& catalog=BuiltinFonts::Catalog();name=(selected&&selected<=catalog.size()?catalog[selected-1].name:options.sources.front().path.stem().u8string())+" "+std::to_string(options.size)+" px";}
        auto job=std::make_shared<Job>();job->action=action;job->name=name;job->settings=Settings(options,w);w->job=job;
        Target(w,action==Action::Apply?w->properties->GetSelectedObject():nullptr);Actions(w,true);
        lv_label_set_text(w->status,"Preparing font... The editor remains responsive.");
        std::thread([job,options] {try{job->result=FontConversion::Prepare(options,&job->cancel);}catch(const std::exception& e){job->result.message=e.what();}job->done.store(true);}).detach();
    } catch(const std::exception& e) {if(w->job)w->job->cancel=true;w->job.reset();Target(w,nullptr);Actions(w,false);lv_label_set_text(w->status,e.what());}
}
void Poll(lv_timer_t* timer) {
    auto* w=static_cast<Window*>(lv_timer_get_user_data(timer));if(!w->job||!w->job->done.load())return;
    auto job=std::move(w->job);Actions(w,false);
    try {
        auto& result=job->result;if(!result.success)throw std::runtime_error(result.message);
        auto* font=lv_binfont_create_from_buffer(result.preview.data(),static_cast<uint32_t>(result.preview.size()));
        if(!font)throw std::runtime_error("The prepared font could not be loaded. Check its character ranges and rendering settings.");
        lv_obj_set_style_text_font(w->preview,font,0);if(w->previewFont)lv_binfont_destroy(w->previewFont);w->previewFont=font;
        if(job->action==Action::Preview) {lv_label_set_text(w->status,"Preview ready. Add this font to keep it in the project.");Target(w,nullptr);return;}
        {
            EditorEdit::Scope edit;
            auto& fonts=w->properties->Fonts();
            if(w->projectFont.empty())w->projectFont=fonts.Add(job->name,result.font,job->settings);
            else {if(w->properties->prepareProjectFontChange)w->properties->prepareProjectFontChange();fonts.Update(w->projectFont,job->name,result.font,job->settings,w->properties->ProjectScreen());}
            if(w->target)fonts.Apply(w->target,w->projectFont);
        }
        const bool applied=w->target!=nullptr;Target(w,nullptr);
        lv_textarea_set_text(w->fields.at("name"),w->properties->Fonts().Record(w->projectFont).at("name").get<std::string>().c_str());
        EditorWindow::SetTitle(w->root,"Edit project font");Actions(w,false);
        w->properties->SetSelectedObject(w->properties->GetSelectedObject(),false);
        const auto message=std::string(applied?"Font saved and applied. ":"Font saved. ")+"It is stored in the project and exported automatically when a screen uses it.";
        lv_label_set_text(w->status,message.c_str());
    } catch(const std::exception& e) {Target(w,nullptr);lv_label_set_text(w->status,e.what());}
}
}
void Close() {if(active)lv_obj_delete(active);}
void OpenFont(PropertyWindow* properties,const std::string& fontId,const std::string& builtinId) {
    if(active) {if(activeWindow->projectFont==fontId&&activeWindow->properties==properties&&(!fontId.empty()||activeWindow->initialBuiltin==builtinId)){lv_obj_move_foreground(active);return;}Close();}
    json record=json::object(),settings=json::object();
    if(!fontId.empty()){record=properties->Fonts().Record(fontId);settings=record.value("conversion",json::object());}
    auto* w=new Window{};w->properties=properties;w->projectFont=fontId;w->initialBuiltin=builtinId;w->root=EditorWindow::Create(lv_layer_top());active=w->root;activeWindow=w;
    lv_obj_set_size(w->root,std::min(940,lv_display_get_horizontal_resolution(nullptr)-40),std::min(900,lv_display_get_vertical_resolution(nullptr)-60));lv_obj_center(w->root);
    EditorWindow::SetTitle(w->root,fontId.empty()?"Add project font":"Edit project font");
    lv_obj_add_event_cb(w->root,[](lv_event_t* event){auto* window=static_cast<Window*>(lv_event_get_user_data(event));active=nullptr;activeWindow=nullptr;delete window;},LV_EVENT_DELETE,w);
    EditorWindow::SetCloseHandler(w->root,[w]{lv_obj_delete(w->root);});
    auto* body=EditorWindow::Content(w->root);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(body,16,0);lv_obj_set_style_pad_row(body,10,0);
    Label(body,"Fonts are stored in this project. Screen exports include only fonts in use. Choose the appearance and characters you need; no output files are required.",LV_PCT(100));
    Input(w,body,"Font name",record.value("name",std::string{}),"name");lv_textarea_set_placeholder_text(w->fields.at("name"),"Automatic from the source font and size");
    Input(w,body,"Size (pixels)",std::to_string(settings.value("size",24)),"size");
    Section(body,"SOURCE FONTS");
    Label(body,"Choose a built-in font or browse to TTF, OTF, WOFF or WOFF2. Add sources to merge characters; later sources replace overlaps. Custom source files must remain available for future changes.",LV_PCT(100));
    w->sources=lv_obj_create(body);lv_obj_set_size(w->sources,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(w->sources,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(w->sources,0,0);lv_obj_set_style_border_width(w->sources,0,0);lv_obj_set_style_bg_opa(w->sources,LV_OPA_TRANSP,0);lv_obj_set_scrollable(w->sources,false);
    Button(body,"Add font source",[w]{AddSource(w);},170);
    Section(body,"APPEARANCE AND MEMORY");
    const int depths[]={1,2,3,4,8};int quality=3;for(int i=0;i<5;++i)if(depths[i]==settings.value("bpp",4))quality=i;
    Choice(w,body,"Glyph quality","1 bit - smallest\n2 bits\n3 bits\n4 bits - recommended\n8 bits - smoothest",quality,"bpp");
    Choice(w,body,"Subpixel smoothing","None (grayscale)\nHorizontal RGB (LCD)\nVertical RGB (LCD)",std::clamp(settings.value("smoothing",0),0,2),"smoothing");
    Check(w,body,"Use color glyph information for grayscale icons",settings.value("colorInfo",false),"colorInfo");
    Check(w,body,"Reduce font storage with compression",settings.value("compress",true),"compress");
    Check(w,body,"Include kerning for natural character spacing",settings.value("kerning",true),"kerning");
    Section(body,"PREVIEW");
    const auto sample=settings.value("sample",std::string("Hello LVGL! 0123456789"));w->sample=Input(w,body,"Preview text",sample);
    w->preview=lv_label_create(body);lv_obj_set_width(w->preview,LV_PCT(100));lv_label_set_text(w->preview,sample.c_str());lv_label_set_long_mode(w->preview,LV_LABEL_LONG_MODE_WRAP);
    if(!fontId.empty())lv_obj_set_style_text_font(w->preview,properties->Fonts().Get(fontId),0);
    lv_obj_add_event_cb(w->sample,[](lv_event_t* event){auto* window=static_cast<Window*>(lv_event_get_user_data(event));lv_label_set_text(window->preview,lv_textarea_get_text(window->sample));},LV_EVENT_VALUE_CHANGED,w);
    auto* actions=Row(body);w->previewButton=Button(actions,"Preview",[w]{Start(w,Action::Preview);},115);
    w->saveButton=Button(actions,"Add to project",[w]{Start(w,Action::Save);},170);w->applyButton=Button(actions,"Add and apply",[w]{Start(w,Action::Apply);},185);Actions(w,false);
    w->status=lv_label_create(body);lv_obj_set_width(w->status,LV_PCT(100));lv_label_set_long_mode(w->status,LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(w->status,fontId.empty()?"Choose a font file, then preview or add it to the project.":"This font is stored in the project. Save changes to update every widget using it.");
    if(settings.contains("sources")&&settings["sources"].is_array())for(const auto& source:settings["sources"])if(source.is_object())AddSource(w,source);
    if(w->rows.empty()&&!builtinId.empty())AddSource(w,json{{"builtinId",builtinId}});
    if(w->rows.empty())AddSource(w);w->timer=lv_timer_create(Poll,100,w);
}
void Open(PropertyWindow* properties,const std::string& fontId){OpenFont(properties,fontId,{});}
void OpenBuiltin(PropertyWindow* properties,const std::string& builtinId){OpenFont(properties,{},builtinId);}
}
