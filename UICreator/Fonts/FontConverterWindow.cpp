#include "FontConverterWindow.h"
#include "FontConverter.h"
#ifndef _WIN32
#include "../Utils/NativeFileDialogUI.h"
#endif
#include "../Windows/PropertyWindow.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include <Serialization/LVFont.h>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <map>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#endif

namespace FontConverterWindow {
namespace {
struct Job {std::atomic<bool> done{false},cancel{false};FontConversion::Result result;std::string name;json settings;};
struct SourceRow {lv_obj_t *root,*path,*ranges,*symbols,*hint;};
struct Window {
    PropertyWindow* properties;
    lv_obj_t *root=nullptr,*sources=nullptr,*status=nullptr,*preview=nullptr,*sample=nullptr,*convert=nullptr,*apply=nullptr;
    std::map<std::string,lv_obj_t*> fields;
    std::vector<SourceRow> rows;
    lv_timer_t* timer=nullptr;
    lv_font_t* previewFont=nullptr;
    std::shared_ptr<Job> job;
    std::string projectFont;
    ~Window() {if(job)job->cancel=true;if(timer)lv_timer_delete(timer);if(previewFont){lv_obj_set_style_text_font(preview,LV_FONT_DEFAULT,0);lv_binfont_destroy(previewFont);}}
};
lv_obj_t* active=nullptr;
void Button(lv_obj_t* parent,const char* label,std::function<void()> action,int width=130) {
    auto* button=lv_button_create(parent);lv_obj_set_size(button,width,36);lv_obj_set_style_shadow_width(button,0,0);
    auto* text=lv_label_create(button);lv_label_set_text(text,label);lv_obj_center(text);
    auto* callback=new std::function<void()>(std::move(action));
    lv_obj_add_event_cb(button,[](lv_event_t* event){auto* fn=static_cast<std::function<void()>*>(lv_event_get_user_data(event));if(lv_event_get_code(event)==LV_EVENT_DELETE){delete fn;return;}if(lv_event_get_code(event)==LV_EVENT_CLICKED){auto action=*fn;action();}},LV_EVENT_ALL,callback);
}
lv_obj_t* Row(lv_obj_t* parent) {
    auto* row=lv_obj_create(parent);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_style_pad_all(row,0,0);lv_obj_set_style_border_width(row,0,0);
    lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);lv_obj_set_style_pad_column(row,10,0);lv_obj_set_scrollable(row,false);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);return row;
}
void Label(lv_obj_t* parent,const char* text,int width=175) {auto* label=lv_label_create(parent);lv_label_set_text(label,text);lv_obj_set_width(label,width);lv_label_set_long_mode(label,LV_LABEL_LONG_MODE_WRAP);}
void Focus(Window* w,lv_obj_t* control) {
    BindEditorControl(w->properties->GetKBGroup(),control);
}
lv_obj_t* Input(Window* w,lv_obj_t* parent,const char* caption,const std::string& initial,const std::string& key="") {
    auto* row=Row(parent);Label(row,caption);
    auto* field=lv_textarea_create(row);lv_textarea_set_one_line(field,true);lv_textarea_set_text(field,initial.c_str());lv_obj_set_height(field,38);lv_obj_set_flex_grow(field,1);Focus(w,field);
    if(!key.empty())w->fields[key]=field;return field;
}
lv_obj_t* Choice(Window* w,lv_obj_t* parent,const char* caption,const char* choices,int selected,const char* key) {
    auto* row=Row(parent);Label(row,caption);auto* field=lv_dropdown_create(row);lv_obj_set_flex_grow(field,1);lv_dropdown_set_options(field,choices);lv_dropdown_set_selected(field,selected);Focus(w,field);if(key&&*key)w->fields[key]=field;return field;
}
lv_obj_t* Check(Window* w,lv_obj_t* parent,const char* caption,bool selected,const char* key) {
    auto* field=lv_checkbox_create(parent);lv_checkbox_set_text(field,caption);lv_obj_set_checked(field,selected);w->fields[key]=field;return field;
}
void Section(lv_obj_t* parent,const char* title) {auto* label=lv_label_create(parent);lv_label_set_text(label,title);lv_obj_set_style_text_color(label,lv_color_hex(0x7bbcff),0);lv_obj_set_style_pad_top(label,8,0);}
std::filesystem::path Pick(bool save,bool folder,const wchar_t* filter,const wchar_t* title,const std::filesystem::path& initial={}) {
#ifdef _WIN32
    if(folder) {
        const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);IFileOpenDialog* dialog=nullptr;std::filesystem::path result;
        if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {
            DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);dialog->SetTitle(title);
            if(SUCCEEDED(dialog->Show(GetActiveWindow()))) {IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){result=path;CoTaskMemFree(path);}item->Release();}}
            dialog->Release();
        }
        if(SUCCEEDED(initialized))CoUninitialize();return result;
    }
    auto initialFile=initial;auto initialDirectory=initial.parent_path();
    if(save&&!initialDirectory.empty()&&!std::filesystem::is_directory(initialDirectory)) {initialFile=initial.filename();initialDirectory=std::filesystem::current_path();}
    wchar_t path[32768]={};auto text=initialFile.wstring();if(text.size()<_countof(path))std::copy(text.begin(),text.end(),path);
    auto directory=initialDirectory.wstring();auto extension=initial.extension().wstring();if(!extension.empty())extension.erase(extension.begin());
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof dialog;dialog.hwndOwner=GetActiveWindow();dialog.lpstrFilter=filter;dialog.lpstrTitle=title;
    dialog.lpstrFile=path;dialog.nMaxFile=_countof(path);dialog.Flags=OFN_NOCHANGEDIR|OFN_EXPLORER|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(!directory.empty())dialog.lpstrInitialDir=directory.c_str();if(save&&!extension.empty())dialog.lpstrDefExt=extension.c_str();
    if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))return std::filesystem::path(path);
#else
    return NativeFileDialogUI::PickOne(save,folder,title,filter,initial);
#endif
    return {};
}
std::string Text(Window* w,const char* key) {return lv_textarea_get_text(w->fields.at(key));}
int Integer(Window* w,const char* key) {const auto text=Text(w,key);size_t end=0;int value=std::stoi(text,&end);if(end!=text.size())throw std::invalid_argument(std::string("Invalid ")+key);return value;}
bool Checked(Window* w,const char* key) {return lv_obj_is_checked(w->fields.at(key));}
void AddSource(Window* w) {
    auto* panel=lv_obj_create(w->sources);lv_obj_set_size(panel,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(panel,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(panel,10,0);lv_obj_set_style_pad_row(panel,8,0);lv_obj_set_scrollable(panel,false);
    SourceRow source{};source.root=panel;source.path=Input(w,panel,"Font file","");
    auto* path=source.path;
    Button(lv_obj_get_parent(path),"Browse...",[w,path] {
        auto selected=Pick(false,false,L"Fonts (*.ttf;*.otf;*.woff;*.woff2)\0*.ttf;*.otf;*.woff;*.woff2\0All files\0*.*\0\0",L"Select source font");
        if(selected.empty())return;lv_textarea_set_text(path,selected.u8string().c_str());
        if(w->rows.size()==1) {
            int size=24;try {size=Integer(w,"size");}catch(const std::exception&){}
            const auto name=FontConversion::SymbolName(selected,size);lv_textarea_set_text(w->fields.at("name"),name.c_str());
            auto output=std::filesystem::current_path()/"generated_fonts"/(name+".c");lv_textarea_set_text(w->fields.at("output"),output.u8string().c_str());
        }
    },110);
    source.ranges=Input(w,panel,"Unicode ranges","0x20-0x7E");lv_textarea_set_placeholder_text(source.ranges,"0x20-0x7E,0x400-0x45F or 0xE000=>0xF000");
    source.symbols=Input(w,panel,"Additional symbols","");
    source.hint=Choice(w,panel,"Hinting","Light (default)\nOff\nStrong",0,"");
    Button(lv_obj_get_parent(source.hint),"Remove",[w,panel] {auto it=std::find_if(w->rows.begin(),w->rows.end(),[panel](const SourceRow& r){return r.root==panel;});if(it!=w->rows.end())w->rows.erase(it);lv_obj_delete(panel);},110);
    w->rows.push_back(source);
}
FontConversion::Options ReadOptions(Window* w) {
    FontConversion::Options o;
    for(const auto& row:w->rows)o.sources.push_back({std::filesystem::u8path(lv_textarea_get_text(row.path)),lv_textarea_get_text(row.ranges),lv_textarea_get_text(row.symbols),int(lv_dropdown_get_selected(row.hint))});
    o.output=std::filesystem::u8path(Text(w,"output"));o.name=Text(w,"name");o.size=Integer(w,"size");
    const char* formats[]={"lvgl","bin","dump"};o.format=formats[lv_dropdown_get_selected(w->fields.at("format"))];
    const int bpp[]={1,2,3,4,8},stride[]={0,1,4,8,16,32,64},align[]={1,4,8,16,32,64,128,256,512,1024};
    o.bpp=bpp[lv_dropdown_get_selected(w->fields.at("bpp"))];o.stride=stride[lv_dropdown_get_selected(w->fields.at("stride"))];o.align=align[lv_dropdown_get_selected(w->fields.at("align"))];
    o.smoothing=lv_dropdown_get_selected(w->fields.at("smoothing"));o.compress=Checked(w,"compress");o.prefilter=Checked(w,"prefilter");o.kerning=Checked(w,"kerning");
    o.fastKerning=Checked(w,"fastKerning");o.colorInfo=Checked(w,"colorInfo");o.fullInfo=Checked(w,"fullInfo");
    o.include=Text(w,"include");o.fallback=Text(w,"fallback");o.relativeRoot=std::filesystem::u8path(Text(w,"relativeRoot"));return o;
}
void Start(Window* w) {
    if(w->job&&!w->job->done)return;
    try {
        auto options=ReadOptions(w);(void)FontConversion::Arguments(options);
        lv_obj_set_disabled(w->convert,true);lv_obj_set_disabled(w->apply,true);lv_label_set_text(w->status,"Converting font... The editor remains responsive.");
        auto job=std::make_shared<Job>();w->job=job;job->name=options.name;job->settings={{"size",options.size},{"bpp",options.bpp},{"compress",options.compress},{"kerning",options.kerning},{"stride",options.stride},{"align",options.align}};
        std::thread([job,options] {job->result=FontConversion::Convert(options,&job->cancel);job->done.store(true);}).detach();
    } catch(const std::exception& e) {lv_label_set_text(w->status,e.what());}
}
void Poll(lv_timer_t* timer) {
    auto* w=static_cast<Window*>(lv_timer_get_user_data(timer));if(!w->job||!w->job->done.load())return;
    auto name=w->job->name;auto settings=w->job->settings;
    auto result=std::move(w->job->result);w->job.reset();lv_obj_set_disabled(w->convert,false);
    if(result.success) {
        auto* font=lv_binfont_create_from_buffer(result.preview.data(),static_cast<uint32_t>(result.preview.size()));
        if(font) {
            w->projectFont=w->properties->Fonts().Add(name,Serialization::LVFont::ToJSON(*font),settings);
            w->properties->AddFontSelector();
            lv_obj_set_style_text_font(w->preview,font,0);if(w->previewFont)lv_binfont_destroy(w->previewFont);w->previewFont=font;lv_obj_set_disabled(w->apply,false);
            result.message+=" Preview ready. Apply to the selected widget to include the font in JSON, Protobuf and generated C.";
        } else result.message+=" Export succeeded; LVGL could not load the preview.";
    }
    lv_label_set_text(w->status,result.message.c_str());
}
}
void Close() {if(active)lv_obj_delete(active);}
void Open(PropertyWindow* properties) {
    if(active) {lv_obj_move_foreground(active);return;}
    auto* w=new Window{};w->properties=properties;w->root=EditorWindow::Create(lv_layer_top());active=w->root;
    lv_obj_set_size(w->root,std::min(980,lv_display_get_horizontal_resolution(nullptr)-40),std::min(920,lv_display_get_vertical_resolution(nullptr)-60));lv_obj_center(w->root);
    EditorWindow::SetTitle(w->root,"Font converter");
    lv_obj_add_event_cb(w->root,[](lv_event_t* event){auto* window=static_cast<Window*>(lv_event_get_user_data(event));active=nullptr;delete window;},LV_EVENT_DELETE,w);
    EditorWindow::SetCloseHandler(w->root,[w]{lv_obj_delete(w->root);});
    auto* body=EditorWindow::Content(w->root);lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(body,16,0);lv_obj_set_style_pad_row(body,10,0);
    Section(body,"SOURCE FONTS");
    Label(body,"TTF, OTF, WOFF and WOFF2. Add sources to merge glyphs; later sources replace overlapping glyphs. Ranges accept decimal, hex and => remapping.",LV_PCT(100));
    w->sources=lv_obj_create(body);lv_obj_set_size(w->sources,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(w->sources,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(w->sources,0,0);lv_obj_set_style_border_width(w->sources,0,0);lv_obj_set_style_bg_opa(w->sources,LV_OPA_TRANSP,0);lv_obj_set_scrollable(w->sources,false);
    Button(body,"Add font source",[w]{AddSource(w);},170);
    Section(body,"OUTPUT");
    Choice(w,body,"Output format","LVGL C source (.c + .h)\nLVGL binary font (.bin)\nGlyph images + JSON (folder)",0,"format");
    Input(w,body,"Font symbol","ui_font_24","name");
    auto* output=Input(w,body,"Output path",(std::filesystem::current_path()/"generated_fonts/ui_font_24.c").u8string(),"output");
    Button(lv_obj_get_parent(output),"Browse...",[w,output] {
        const auto format=lv_dropdown_get_selected(w->fields.at("format"));
        auto path=Pick(true,format==2,format==0?L"C source (*.c)\0*.c\0\0":L"Binary font (*.bin)\0*.bin\0\0",L"Choose font output",std::filesystem::u8path(lv_textarea_get_text(output)));
        if(!path.empty()){if(format==0)path.replace_extension(".c");else if(format==1)path.replace_extension(".bin");lv_textarea_set_text(output,path.u8string().c_str());}
    },110);
    lv_obj_add_event_cb(w->fields.at("format"),[](lv_event_t* event){auto* window=static_cast<Window*>(lv_event_get_user_data(event));auto format=lv_dropdown_get_selected(lv_event_get_target_obj(event));auto path=std::filesystem::u8path(Text(window,"output"));if(format<2)path.replace_extension(format==0?".c":".bin");else path.replace_extension();lv_textarea_set_text(window->fields.at("output"),path.u8string().c_str());},LV_EVENT_VALUE_CHANGED,w);
    Input(w,body,"Size (pixels)","24","size");Choice(w,body,"Bits per pixel","1\n2\n3\n4\n8",3,"bpp");
    Section(body,"RASTERIZATION AND SIZE");
    Choice(w,body,"Subpixel smoothing","None (grayscale)\nHorizontal RGB (LCD)\nVertical RGB (LCD)",0,"smoothing");
    Check(w,body,"Use color glyph information for grayscale icons",false,"colorInfo");
    Check(w,body,"Compress glyph bitmaps (RLE)",true,"compress");Check(w,body,"XOR prefilter for compression",true,"prefilter");
    Check(w,body,"Include kerning",true,"kerning");Check(w,body,"Force fast kerning classes (may be larger)",false,"fastKerning");
    Choice(w,body,"Row stride alignment","0 (packed)\n1 byte\n4 bytes\n8 bytes\n16 bytes\n32 bytes\n64 bytes",0,"stride");
    Choice(w,body,"Glyph address alignment","1 byte\n4 bytes\n8 bytes\n16 bytes\n32 bytes\n64 bytes\n128 bytes\n256 bytes\n512 bytes\n1024 bytes",0,"align");
    Label(body,"Stride alignment requires compression off and 1, 2, 4 or 8 bpp. Preview uses uncompressed packed glyphs; exported files use the settings above.",LV_PCT(100));
    Section(body,"C OUTPUT AND DEBUG OPTIONS");
    Input(w,body,"LVGL include path","lvgl.h","include");Input(w,body,"Fallback font symbol","","fallback");Input(w,body,"Project root (optional)","","relativeRoot");
    Check(w,body,"Full glyph information in debug JSON",false,"fullInfo");
    Label(body,"Fallback symbol and include path affect C export. Firmware must link the named fallback. Enable LV_USE_FONT_COMPRESSED for compressed C fonts and LV_FONT_FMT_TXT_LARGE when glyph metrics exceed the small descriptor limits.",LV_PCT(100));
    Section(body,"PREVIEW");
    w->sample=Input(w,body,"Preview text","Hello LVGL! 0123456789");
    w->preview=lv_label_create(body);lv_obj_set_width(w->preview,LV_PCT(100));lv_label_set_text(w->preview,"Hello LVGL! 0123456789");lv_label_set_long_mode(w->preview,LV_LABEL_LONG_MODE_WRAP);
    lv_obj_add_event_cb(w->sample,[](lv_event_t* event){auto* window=static_cast<Window*>(lv_event_get_user_data(event));lv_label_set_text(window->preview,lv_textarea_get_text(window->sample));},LV_EVENT_VALUE_CHANGED,w);
    auto* actions=Row(body);Button(actions,"Convert and export",[w]{Start(w);},190);w->convert=lv_obj_get_child(actions,-1);
    Button(actions,"Apply to selected widget",[w] {
        try {
            auto* object=w->properties->GetSelectedObject();if(!object||!w->previewFont)throw std::runtime_error("Select a widget and convert a font first");
            w->properties->Fonts().Apply(object,w->projectFont);w->properties->SetSelectedObject(object);
            lv_label_set_text(w->status,"Font applied. Saving or exporting the project includes its bitmap and kerning.");
        }catch(const std::exception& e){lv_label_set_text(w->status,e.what());}
    },240);w->apply=lv_obj_get_child(actions,-1);lv_obj_set_disabled(w->apply,true);
    w->status=lv_label_create(body);lv_obj_set_width(w->status,LV_PCT(100));lv_label_set_long_mode(w->status,LV_LABEL_LONG_MODE_WRAP);lv_label_set_text(w->status,"Choose a font file and output path, then convert.");
    AddSource(w);w->timer=lv_timer_create(Poll,100,w);
}
}
