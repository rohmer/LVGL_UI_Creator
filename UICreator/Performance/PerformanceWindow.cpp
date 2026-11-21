#include "PerformanceWindow.h"
#ifndef _WIN32
#include "../Utils/NativeFileDialogUI.h"
#endif
#include "Telemetry.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorSession.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif
namespace PerformanceWindow {
namespace {
using namespace Performance;
struct Window {
    Context context;lv_obj_t *root=nullptr,*source=nullptr,*summary=nullptr,*table=nullptr,*memory=nullptr,*budgetStatus=nullptr,*status=nullptr;
    lv_obj_t *enabled=nullptr,*preset=nullptr,*ram=nullptr,*flash=nullptr,*frame=nullptr,*prediction=nullptr,*prefix=nullptr;
    lv_timer_t* timer=nullptr;Snapshot imported,last;bool usingDevice=false;uint64_t firmwareBytes=0;bool firmwareSelected=false;
    ~Window(){if(timer)lv_timer_delete(timer);}
};
Window* active=nullptr;
std::string Number(double number,int decimals=2){std::ostringstream value;value<<std::fixed<<std::setprecision(decimals)<<number;return value.str();}
std::string Bytes(uint64_t value){if(value>=1024u*1024u)return Number(double(value)/(1024.0*1024.0))+" MiB";if(value>=1024)return Number(double(value)/1024.0)+" KiB";return std::to_string(value)+" B";}
lv_obj_t* Label(lv_obj_t* parent,const std::string& text){auto* label=lv_label_create(parent);lv_label_set_text(label,text.c_str());lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);return label;}
lv_obj_t* Row(lv_obj_t* parent){auto* row=lv_obj_create(parent);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_all(row,0,0);lv_obj_set_style_border_width(row,0,0);lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);lv_obj_set_style_pad_column(row,8,0);lv_obj_set_style_pad_row(row,8,0);lv_obj_set_scrollable(row,false);return row;}
void Focus(Window* w,lv_obj_t* field){lv_obj_add_event_cb(field,[](lv_event_t* event){auto* state=static_cast<Window*>(lv_event_get_user_data(event));FocusEditorControl(state->context.keyboardGroup,lv_event_get_target_obj(event));},LV_EVENT_CLICKED,w);}
lv_obj_t* Button(Window* w,lv_obj_t* parent,const char* text,std::function<void()> action,int width=170){auto* button=lv_button_create(parent);lv_obj_set_size(button,width,36);EditorStyle::Button(button);auto* label=lv_label_create(button);lv_label_set_text(label,text);lv_obj_center(label);lv_obj_add_event_cb(button,[](lv_event_t* event){auto* fn=static_cast<std::function<void()>*>(lv_event_get_user_data(event));if(lv_event_get_code(event)==LV_EVENT_DELETE)delete fn;else if(lv_event_get_code(event)==LV_EVENT_CLICKED){auto copy=*fn;copy();}},LV_EVENT_ALL,new std::function<void()>(std::move(action)));Focus(w,button);return button;}
lv_obj_t* Field(Window* w,lv_obj_t* parent,const char* caption,const char* value){Label(parent,caption);auto* field=lv_textarea_create(parent);lv_obj_set_size(field,LV_PCT(100),36);lv_textarea_set_one_line(field,true);lv_textarea_set_text(field,value);EditorStyle::Control(field);Focus(w,field);return field;}
std::filesystem::path Pick(bool firmware){
#ifdef _WIN32
    wchar_t name[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof dialog;dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=name;dialog.nMaxFile=32768;
    dialog.lpstrFilter=firmware?L"Firmware binary (*.bin;*.elf)\0*.bin;*.elf\0All files\0*.*\0\0":L"Device measurements (*.json)\0*.json\0All files\0*.*\0\0";
    dialog.lpstrTitle=firmware?L"Select the compiled firmware image":L"Load actual device measurements";dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetOpenFileNameW(&dialog))return name;
#else
    return NativeFileDialogUI::PickOne(false,false,firmware?L"Select the compiled firmware image":L"Load actual device measurements",firmware?L"Firmware binary\0*.bin;*.elf\0All files\0*.*\0\0":L"Device measurements\0*.json\0All files\0*.*\0\0");
#endif
    return {};
}
double Value(lv_obj_t* field,const char* name,bool whole=false){std::string text=lv_textarea_get_text(field);size_t used=0;double number=0;try{number=std::stod(text,&used);}catch(...){throw std::invalid_argument(std::string("Enter a valid nonnegative ")+name);}if(used!=text.size()||!std::isfinite(number)||number<0||number>1024.0*1024.0*1024.0||(whole&&number!=std::floor(number)))throw std::invalid_argument(std::string("Enter a valid nonnegative ")+name);return number;}
Budget ReadBudget(Window* w){Budget out;const auto selected=lv_dropdown_get_selected(w->preset);out.name=BudgetPresets().at(selected).name;out.ramBytes=uint64_t(Value(w->ram,"RAM budget",true))*1024u;out.flashBytes=uint64_t(Value(w->flash,"flash budget",true))*1024u;out.frameMs=Value(w->frame,"frame budget");out.predictionMs=Value(w->prediction,"prediction budget");return out;}
void ShowBudget(Window* w,const Budget& budget){lv_textarea_set_text(w->ram,std::to_string(budget.ramBytes/1024u).c_str());lv_textarea_set_text(w->flash,std::to_string(budget.flashBytes/1024u).c_str());lv_textarea_set_text(w->frame,Number(budget.frameMs,3).c_str());lv_textarea_set_text(w->prediction,Number(budget.predictionMs,3).c_str());}
std::filesystem::path Settings(){return EditorSession::DefaultDirectory()/"performance-budgets.json";}
void SaveBudget(Window* w){const auto budget=ReadBudget(w);json settings;try{std::ifstream file(Settings());if(file)file>>settings;}catch(...){}if(!settings.is_object())settings=json::object();settings["selected"]=lv_dropdown_get_selected(w->preset);settings["presets"][budget.name]={{"ramBytes",budget.ramBytes},{"flashBytes",budget.flashBytes},{"frameMs",budget.frameMs},{"predictionMs",budget.predictionMs}};EditorSession::WriteAtomic(Settings(),settings.dump(2));}
void LoadBudget(Window* w,unsigned selected){auto budget=BudgetPresets().at(selected);try{std::ifstream file(Settings());json settings;if(file){file>>settings;const auto saved=settings.value("presets",json::object()).value(budget.name,json::object());if(!saved.empty()){budget.ramBytes=Counter(saved,"ramBytes");budget.flashBytes=Counter(saved,"flashBytes");budget.frameMs=saved.value("frameMs",budget.frameMs);budget.predictionMs=saved.value("predictionMs",budget.predictionMs);if(!std::isfinite(budget.frameMs)||budget.frameMs<0||!std::isfinite(budget.predictionMs)||budget.predictionMs<0)budget=BudgetPresets().at(selected);}}}catch(...){}ShowBudget(w,budget);}
void Refresh(Window* w){
    try {
        w->last=w->usingDevice?w->imported:w->context.snapshot?w->context.snapshot():Snapshot{};
        if(w->firmwareSelected){w->last.firmwareAvailable=true;w->last.firmwareBytes=w->firmwareBytes;}
        const auto& s=w->last;
        const auto source=s.deviceMeasurement?s.source+": "+s.device:s.source+" | host measurements, not physical target timing";
        lv_label_set_text(w->source,source.c_str());
        const auto summary=!s.running&&!s.deviceMeasurement?"Start Run mode (F6) to collect a screen, or load a device measurement file.":
            "Frames drawn: "+std::to_string(s.frames)+"  |  Actual redraw FPS: "+(s.fpsAvailable?Number(s.fps):std::string("unavailable"))+"  |  Loaded screens: "+std::to_string(s.loadedScreens)+" / "+std::to_string(s.totalScreens)+
            "\nScreen load: "+Number(s.loadMs)+" ms  |  Latest lazy load: "+Number(s.lastScreenLoadMs)+" ms  |  Loaded authored widgets: "+std::to_string(s.loadedObjects);
        lv_label_set_text(w->summary,summary.c_str());
        const Distribution* values[]={&s.frame,&s.render,&s.flush,&s.key,&s.prediction};
        const char* names[]={"Frame CPU work","Render","Flush callback","Input processing","Prediction lookup"};
        for(unsigned i=0;i<5;++i){lv_table_set_cell_value(w->table,i+1,0,names[i]);const auto& d=*values[i];lv_table_set_cell_value(w->table,i+1,1,std::to_string(d.samples).c_str());for(unsigned column=2;column<6;++column){const double value=column==2?d.mean:column==3?d.p50:column==4?d.p95:d.maximum;lv_table_set_cell_value(w->table,i+1,column,d.samples?Number(value,3).c_str():"unavailable");}}
        std::string memory="Input Protobuf: "+Bytes(s.inputBytes)+"  |  Host framebuffer: "+Bytes(s.framebufferBytes)+"  |  Redrawn pixels: "+std::to_string(s.redrawPixels);
        memory+=s.heapAvailable?"\nWhole LVGL allocator: used "+Bytes(s.heapUsed)+", peak "+Bytes(s.heapPeak)+", largest free block "+Bytes(s.largestFree):"\nLVGL allocator monitoring: unavailable for this allocator";
        memory+="\nLVGL heap figures exclude C++ resources and the host framebuffer, and include other editor windows. They are not target RAM usage.";
        lv_label_set_text(w->memory,memory.c_str());
        const auto budget=ReadBudget(w);std::string result="Budget: "+budget.name;
        if(budget.frameMs>0&&s.frame.samples)result+="\nFrame P95 "+Number(s.frame.p95)+" / "+Number(budget.frameMs)+" ms: "+(s.frame.p95>budget.frameMs?"OVER BUDGET":"within budget");
        if(budget.predictionMs>0&&s.prediction.samples)result+="\nPrediction P95 "+Number(s.prediction.p95)+" / "+Number(budget.predictionMs)+" ms: "+(s.prediction.p95>budget.predictionMs?"OVER BUDGET":"within budget");
        if(budget.ramBytes)result+=s.deviceMeasurement&&s.ramAvailable?"\nReported target RAM peak "+Bytes(s.ramPeak)+" / "+Bytes(budget.ramBytes)+": "+(s.ramPeak>budget.ramBytes?"OVER BUDGET":"within budget"):"\nTarget RAM: load actual device measurements to check this budget";
        if(budget.flashBytes)result+=s.firmwareAvailable?"\nSelected image "+Bytes(s.firmwareBytes)+" / "+Bytes(budget.flashBytes)+": "+(s.firmwareBytes>budget.flashBytes?"OVER BUDGET":"within budget"):"\nFlash: select a compiled image to check its file size";
        lv_label_set_text(w->budgetStatus,result.c_str());
    }catch(const std::exception& error){lv_label_set_text(w->status,error.what());}
}
}
lv_obj_t* ActiveWindow(){return active?active->root:nullptr;}
void Close(){if(!active)return;auto* w=active;auto callback=w->context.enable;try{SaveBudget(w);}catch(...){}EditorWindow::ReleaseFocus(w->root);lv_obj_delete(w->root);if(callback)callback(false);}
void Open(Context context){
    if(active){lv_obj_move_foreground(active->root);return;}
    auto* w=new Window;w->context=std::move(context);active=w;w->root=EditorWindow::Create(lv_layer_top());
    lv_obj_set_size(w->root,(std::min)(1040,lv_display_get_horizontal_resolution(nullptr)-40),(std::min)(850,lv_display_get_vertical_resolution(nullptr)-60));lv_obj_center(w->root);EditorWindow::SetTitle(w->root,"Performance dashboard");EditorWindow::SetCloseHandler(w->root,[]{Close();});
    lv_obj_add_event_cb(w->root,[](lv_event_t* event){auto* state=static_cast<Window*>(lv_event_get_user_data(event));if(active==state)active=nullptr;delete state;},LV_EVENT_DELETE,w);
    auto* content=EditorWindow::Content(w->root);lv_obj_set_flex_flow(content,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(content,14,0);lv_obj_set_style_pad_row(content,10,0);
    w->source=Label(content,"");auto* actions=Row(content);w->enabled=lv_checkbox_create(actions);lv_checkbox_set_text(w->enabled,"Collect host timings");lv_obj_add_state(w->enabled,LV_STATE_CHECKED);Focus(w,w->enabled);
    lv_obj_add_event_cb(w->enabled,[](lv_event_t* event){auto* state=static_cast<Window*>(lv_event_get_user_data(event));if(state->context.enable)state->context.enable(lv_obj_has_state(state->enabled,LV_STATE_CHECKED));},LV_EVENT_VALUE_CHANGED,w);
    Button(w,actions,"Use host preview",[w]{w->usingDevice=false;Refresh(w);});
    Button(w,actions,"Load device sample",[w]{try{const auto path=Pick(false);if(path.empty())return;if(std::filesystem::file_size(path)>256u*1024u)throw std::invalid_argument("Device measurement file exceeds 256 KiB");std::ifstream file(path);json data;file>>data;w->imported=DeviceSnapshot(data);w->usingDevice=true;Refresh(w);lv_label_set_text(w->status,"Loaded actual device samples. Host and device results remain separate.");}catch(const std::exception& error){lv_label_set_text(w->status,error.what());}},180);
    Button(w,actions,"Copy report",[w]{Refresh(w);const auto data=Report(w->last).dump(2);lv_label_set_text(w->status,SDL_SetClipboardText(data.c_str())?"Measurement report copied; no project content was included.":SDL_GetError());},180);
    w->summary=Label(content,"");w->table=lv_table_create(content);lv_obj_set_width(w->table,LV_PCT(100));lv_table_set_column_count(w->table,6);lv_table_set_row_count(w->table,6);
    for(unsigned i=0;i<6;++i){const char* headings[]={"Measured scope","Samples","Mean ms","P50 ms","P95 ms","Max ms"};lv_table_set_cell_value(w->table,0,i,headings[i]);lv_table_set_column_width(w->table,i,i?130:215);}lv_obj_set_style_pad_all(w->table,6,LV_PART_ITEMS);Focus(w,w->table);
    Label(content,"Timings retain the latest 256 samples. Idle screens do not redraw, so redraw FPS can be zero. Flush measures the host callback; it does not include a physical panel transfer. No background timing work is performed when collection is off.");
    w->memory=Label(content,"");
    auto* benchmark=Row(content);auto* prefixLabel=lv_label_create(benchmark);lv_label_set_text(prefixLabel,"Prediction prefix");w->prefix=lv_textarea_create(benchmark);lv_textarea_set_one_line(w->prefix,true);lv_textarea_set_max_length(w->prefix,128);lv_textarea_set_text(w->prefix,"hel");lv_obj_set_size(w->prefix,190,36);Focus(w,w->prefix);
    Button(w,benchmark,"Measure 100 lookups",[w]{try{if(!w->context.benchmark)throw std::invalid_argument("Prediction measurement is unavailable");const auto result=w->context.benchmark(lv_textarea_get_text(w->prefix),100);lv_label_set_text(w->status,("Measured "+std::to_string(result.samples)+" real predictor lookups; P95 "+Number(result.p95,3)+" ms. Typing content was unchanged.").c_str());Refresh(w);}catch(const std::exception& error){lv_label_set_text(w->status,error.what());}},200);
    Label(content,"Application budgets (editable starting points; reserve memory for your own OS, drivers and application)");w->preset=lv_dropdown_create(content);std::string names;for(const auto& preset:BudgetPresets()){if(!names.empty())names+='\n';names+=preset.name;}lv_dropdown_set_options(w->preset,names.c_str());lv_obj_set_width(w->preset,LV_PCT(100));Focus(w,w->preset);
    w->ram=Field(w,content,"Target measured RAM budget (KiB; 0 disables)","0");w->flash=Field(w,content,"Compiled image budget (KiB; 0 disables)","0");w->frame=Field(w,content,"Frame CPU P95 budget (ms)","33.333");w->prediction=Field(w,content,"Prediction P95 budget (ms)","10");
    lv_obj_add_event_cb(w->preset,[](lv_event_t* event){auto* state=static_cast<Window*>(lv_event_get_user_data(event));LoadBudget(state,lv_dropdown_get_selected(state->preset));Refresh(state);},LV_EVENT_VALUE_CHANGED,w);
    auto* budgetActions=Row(content);Button(w,budgetActions,"Save budget",[w]{try{SaveBudget(w);Refresh(w);lv_label_set_text(w->status,"Named application budget saved for this session profile.");}catch(const std::exception& error){lv_label_set_text(w->status,error.what());}});
    Button(w,budgetActions,"Select compiled image",[w]{try{const auto path=Pick(true);if(path.empty())return;w->firmwareBytes=std::filesystem::file_size(path);w->firmwareSelected=true;Refresh(w);lv_label_set_text(w->status,"Image file size selected. ELF includes debug/symbol data; choose the flashed .bin for a flash budget.");}catch(const std::exception& error){lv_label_set_text(w->status,error.what());}},200);
    w->budgetStatus=Label(content,"");w->status=Label(content,"");unsigned selected=0;try{std::ifstream file(Settings());json data;if(file){file>>data;selected=unsigned(Counter(data,"selected"));if(selected>=BudgetPresets().size())selected=0;}}catch(...){}lv_dropdown_set_selected(w->preset,selected);LoadBudget(w,selected);
    if(w->context.enable)w->context.enable(true);Refresh(w);w->timer=lv_timer_create([](lv_timer_t* timer){Refresh(static_cast<Window*>(lv_timer_get_user_data(timer)));},250,w);
}
}
