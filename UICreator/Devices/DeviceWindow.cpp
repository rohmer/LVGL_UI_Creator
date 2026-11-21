#include "DeviceWindow.h"
#ifndef _WIN32
#include "../Utils/NativeFileDialogUI.h"
#endif
#include "DeviceTools.h"
#include "DeviceTargets.h"
#include "../Validation/ScreenValidation.h"
#include "../Windows/PropertyWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorSession.h"
#include "../Utils/EditorWindow.h"
#include <array>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#endif

namespace DeviceWindow {
namespace {
constexpr size_t LogLimit=128*1024;
struct Work {
    std::atomic_bool cancel{false},done{false};
    std::mutex mutex;
    std::string log;
    size_t revision=0;
    DeviceTargets::Progress progress;
    DeviceTools::Result result;
    void Append(const std::string& text) {
        std::lock_guard<std::mutex> lock(mutex);
        progress.Feed(text);
        // Text widgets need printable text. Keep LF/TAB and UTF-8, remove serial
        // NUL/other control bytes and render carriage-return progress as lines.
        for(unsigned char c:text){if(c=='\r')log+='\n';else if(c=='\n'||c=='\t'||c>=32)log+=char(c);}
        if(log.size()>LogLimit){size_t start=log.size()-LogLimit;while(start<log.size()&&(static_cast<unsigned char>(log[start])&0xc0)==0x80)++start;log.erase(0,start);}
        ++revision;
    }
};
struct Job {
    std::shared_ptr<Work> work=std::make_shared<Work>();
    std::thread thread;
    ~Job(){work->cancel=true;if(thread.joinable())thread.join();}
};
struct Window {
    PropertyWindow* properties=nullptr;
    Context context;
    lv_obj_t *root=nullptr,*profile=nullptr,*project=nullptr,*build=nullptr,*tool=nullptr,*python=nullptr,*idf=nullptr,*idfTools=nullptr,*toolPath=nullptr,*config=nullptr,*library=nullptr,*fqbn=nullptr,*port=nullptr,*baud=nullptr,*jobs=nullptr;
    lv_obj_t *status=nullptr,*configStatus=nullptr,*ports=nullptr,*constraints=nullptr,*log=nullptr,*serialLog=nullptr,*serialStatus=nullptr,*buildButton=nullptr,*flashButton=nullptr,*stopButton=nullptr,*connectButton=nullptr,*disconnectButton=nullptr,*probeButton=nullptr,*progress=nullptr,*tabs=nullptr;
    lv_obj_t *targetChoice=nullptr,*targetName=nullptr,*wsl=nullptr,*runProgram=nullptr,*runButton=nullptr,*phase=nullptr,*spinner=nullptr;
    std::vector<std::string> targetIds;
    lv_timer_t* timer=nullptr;
    std::array<DeviceTools::Options,3> settings;
    unsigned current=0;
    std::unique_ptr<Job> job,monitor;
    size_t jobRevision=0,serialRevision=0;
    ~Window(){if(timer)lv_timer_delete(timer);job.reset();monitor.reset();}
};
Window* active=nullptr;
Context configured;
bool shutdown=false;
std::filesystem::path Workspace(){auto p=std::filesystem::current_path();for(int i=0;i<5;++i){if(std::filesystem::exists(p/"UICreator")&&std::filesystem::exists(p/"CMakeLists.txt"))return p;if(p==p.parent_path())break;p=p.parent_path();}return std::filesystem::current_path();}
std::filesystem::path SettingsPath(){return EditorSession::DefaultDirectory()/"device-tools.json";}
void LoadSettings(Window* w){
    try{std::ifstream stream(SettingsPath(),std::ios::binary);if(!stream)return;const auto json=nlohmann::json::parse(stream);w->current=(std::min)(2u,json.value("profile",0u));
        const auto& profiles=json.at("profiles");for(size_t i=0;i<3&&i<profiles.size();++i){auto value=profiles[i];value["profile"]=i;w->settings[i]=DeviceTargets::Restore(value);}
    }catch(...){/* Damaged session settings must not block the editor. */}
}
void SaveSettings(Window* w){
    nlohmann::json profiles=nlohmann::json::array();for(const auto& o:w->settings)profiles.push_back(DeviceTargets::Serialize(o));
    std::filesystem::create_directories(SettingsPath().parent_path());EditorSession::WriteAtomic(SettingsPath(),nlohmann::json({{"profile",w->current},{"profiles",profiles}}).dump(2));
}
unsigned Number(lv_obj_t* field,unsigned minimum,unsigned maximum,const char* label){const auto text=std::string(lv_textarea_get_text(field));size_t end=0;unsigned long result=0;try{result=std::stoul(text,&end);}catch(...){throw std::invalid_argument(std::string("Enter a numeric ")+label);}if(end!=text.size()||result<minimum||result>maximum)throw std::invalid_argument(std::string("Invalid ")+label);return unsigned(result);}
DeviceTools::Options Read(Window* w){auto o=w->settings[w->current];o.profile=DeviceTools::Profile(w->current);auto path=[](lv_obj_t* field){return std::filesystem::u8path(lv_textarea_get_text(field));};o.project=path(w->project);o.build=path(w->build);o.executable=path(w->tool);o.python=path(w->python);o.idfRoot=path(w->idf);o.idfTools=path(w->idfTools);o.config=path(w->config);o.library=path(w->library);o.toolPath=lv_textarea_get_text(w->toolPath);o.fqbn=lv_textarea_get_text(w->fqbn);o.port=lv_textarea_get_text(w->port);o.baud=Number(w->baud,1,4000000,"baud rate");o.jobs=Number(w->jobs,1,256,"worker count");o.checkIdfConstraints=lv_obj_has_state(w->constraints,LV_STATE_CHECKED);o.linuxHostWsl=lv_obj_has_state(w->wsl,LV_STATE_CHECKED);o.runProgram=path(w->runProgram);const auto oldBuild=w->settings[w->current].build.empty()?w->settings[w->current].project/"build":w->settings[w->current].build;const auto newBuild=o.build.empty()?o.project/"build":o.build;if(!oldBuild.empty()&&oldBuild!=newBuild){const auto relative=o.runProgram.lexically_relative(oldBuild);if(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..")o.runProgram=newBuild/relative;}return o;}
void Show(Window* w,const DeviceTools::Options& o){auto path=[](lv_obj_t* f,const std::filesystem::path& p){lv_textarea_set_text(f,p.u8string().c_str());};path(w->project,o.project);path(w->build,o.build);path(w->tool,o.executable);path(w->python,o.python);path(w->idf,o.idfRoot);path(w->idfTools,o.idfTools);path(w->config,o.config);path(w->library,o.library);lv_textarea_set_text(w->toolPath,o.toolPath.c_str());lv_textarea_set_text(w->fqbn,o.fqbn.c_str());lv_textarea_set_text(w->port,o.port.c_str());lv_textarea_set_text(w->baud,std::to_string(o.baud).c_str());lv_textarea_set_text(w->jobs,std::to_string(o.jobs).c_str());lv_obj_set_state(w->constraints,LV_STATE_CHECKED,o.checkIdfConstraints);lv_obj_set_state(w->wsl,LV_STATE_CHECKED,o.linuxHostWsl);path(w->runProgram,o.runProgram);}
lv_obj_t* Row(lv_obj_t* parent){auto* row=lv_obj_create(parent);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_all(row,0,0);lv_obj_set_style_border_width(row,0,0);lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);lv_obj_set_style_pad_column(row,8,0);lv_obj_set_scrollable(row,false);lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);return row;}
void Focus(Window* w,lv_obj_t* control){lv_obj_add_event_cb(control,[](lv_event_t* e){auto* w=static_cast<Window*>(lv_event_get_user_data(e));FocusEditorControl(w->properties->GetKBGroup(),lv_event_get_target_obj(e));},LV_EVENT_CLICKED,w);}
lv_obj_t* Button(Window* w,lv_obj_t* parent,const char* text,std::function<void()> action,int width=120){auto* button=lv_button_create(parent);lv_obj_set_size(button,width,34);EditorStyle::Button(button);auto* label=lv_label_create(button);lv_label_set_text(label,text);lv_obj_center(label);Focus(w,button);lv_obj_add_event_cb(button,[](lv_event_t* e){auto* f=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete f;else if(lv_event_get_code(e)==LV_EVENT_CLICKED){auto action=*f;action();}},LV_EVENT_ALL,new std::function<void()>(std::move(action)));return button;}
std::filesystem::path Pick(bool folder){
#ifdef _WIN32
    if(folder){const auto init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);IFileOpenDialog* dialog=nullptr;std::filesystem::path result;if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))){DWORD flags=0;dialog->GetOptions(&flags);dialog->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);dialog->SetTitle(L"Select project or toolchain folder");if(SUCCEEDED(dialog->Show(GetActiveWindow()))){IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){result=path;CoTaskMemFree(path);}item->Release();}}dialog->Release();}if(SUCCEEDED(init))CoUninitialize();return result;}
    wchar_t path[32768]{};OPENFILENAMEW d{};d.lStructSize=sizeof d;d.hwndOwner=GetActiveWindow();d.lpstrFile=path;d.nMaxFile=32768;d.lpstrFilter=L"Tools and configuration\0*.exe;*.yaml;*.yml\0All files\0*.*\0\0";d.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST;if(GetOpenFileNameW(&d))return path;
#else
    return NativeFileDialogUI::PickOne(false,folder,folder?L"Select project or toolchain folder":L"Select tool or configuration",L"All files\0*.*\0\0");
#endif
    return {};
}
lv_obj_t* Field(Window* w,lv_obj_t* parent,const char* name,const char* hint,bool browse=false,bool folder=false){auto* row=Row(parent);auto* label=lv_label_create(row);lv_label_set_text(label,name);lv_obj_set_width(label,160);auto* field=lv_textarea_create(row);lv_obj_set_height(field,34);lv_obj_set_flex_grow(field,1);lv_textarea_set_one_line(field,true);lv_textarea_set_placeholder_text(field,hint);EditorStyle::Control(field);Focus(w,field);if(browse)Button(w,row,"Browse...",[field,folder]{auto p=Pick(folder);if(!p.empty())lv_textarea_set_text(field,p.u8string().c_str());},90);return field;}
void Status(Window* w,const std::string& text){lv_label_set_text(w->status,text.c_str());if(w->configStatus)lv_label_set_text(w->configStatus,text.c_str());}
void RefreshTargets(Window* w,const std::string& selected={}){
    w->targetIds={""};std::string labels="Unsaved target";unsigned choice=0;
    if(w->context.document){const auto document=w->context.document();for(const auto& target:document.value("deviceTargets",nlohmann::json::array()))if(target.is_object()&&target.contains("id")&&target.at("id").is_string()){const auto id=target.at("id").get<std::string>();if(id==selected)choice=unsigned(w->targetIds.size());w->targetIds.push_back(id);auto name=target.contains("name")&&target["name"].is_string()?target["name"].get<std::string>():id;for(auto& c:name)if(c=='\n'||c=='\r')c=' ';labels+='\n'+name;}}
    lv_dropdown_set_options(w->targetChoice,labels.c_str());lv_dropdown_set_selected(w->targetChoice,choice);
}
void SelectTarget(Window* w){
    if(!w->context.document)return;const auto choice=lv_dropdown_get_selected(w->targetChoice);if(choice==0||choice>=w->targetIds.size())return;
    const auto document=w->context.document();for(const auto& target:document.value("deviceTargets",nlohmann::json::array()))if(target.value("id",std::string{})==w->targetIds[choice]){
        auto options=DeviceTools::Detect(DeviceTargets::Restore(target.at("options")),Workspace());w->current=unsigned(options.profile);w->settings[w->current]=options;lv_dropdown_set_selected(w->profile,w->current);Show(w,options);lv_textarea_set_text(w->targetName,target.value("name",std::string{}).c_str());Status(w,"Target loaded. Build, Run and Flash require their explicit buttons.");break;
    }
}
void CommitTarget(Window* w,bool copy,bool remove=false){
    try{
        if(!w->context.document||(!w->context.applyMetadata&&!w->context.apply))throw std::runtime_error("Target presets require an open authoring project");
        auto document=w->context.document();const auto choice=lv_dropdown_get_selected(w->targetChoice);auto id=choice<w->targetIds.size()?w->targetIds[choice]:std::string{};
        const auto options=Read(w);if(remove){if(id.empty())throw std::runtime_error("Select a saved target to remove");DeviceTargets::Remove(document,id);id.clear();}
        else id=DeviceTargets::Save(document,lv_textarea_get_text(w->targetName),options,copy?std::string{}:id);
        auto context=w->context;auto* properties=w->properties;
        if(context.applyMetadata)context.applyMetadata({{"deviceTargets",document.value("deviceTargets",nlohmann::json::array())}});
        else context.apply(document);
        if(active!=w){Open(properties);w=active;w->current=unsigned(options.profile);w->settings[w->current]=options;lv_dropdown_set_selected(w->profile,w->current);Show(w,options);}
        RefreshTargets(w,id);Status(w,remove?"Target removed. Undo is available.":"Project target saved. Undo is available.");
    }catch(const std::exception& error){if(active==w)Status(w,error.what());}
}
void RefreshButtons(Window* w){const bool busy=w->job&&!w->job->work->done.load();const bool monitor=w->monitor&&!w->monitor->work->done.load();lv_obj_set_disabled(w->buildButton,busy);lv_obj_set_disabled(w->probeButton,busy);lv_obj_set_disabled(w->flashButton,busy||w->current==2);lv_obj_set_disabled(w->stopButton,!busy);lv_obj_set_disabled(w->profile,busy);lv_obj_set_disabled(w->connectButton,monitor||busy);lv_obj_set_disabled(w->disconnectButton,!monitor);
    lv_obj_set_disabled(w->targetChoice,busy);lv_obj_set_disabled(w->runButton,busy||w->current!=2||std::string(lv_textarea_get_text(w->runProgram)).empty());lv_obj_set_hidden(w->wsl,w->current!=2);lv_obj_set_hidden(lv_obj_get_parent(w->runProgram),w->current!=2);lv_obj_set_hidden(w->constraints,w->current!=1);lv_obj_set_hidden(lv_obj_get_parent(w->tool),w->current==1||(w->current==2&&lv_obj_has_state(w->wsl,LV_STATE_CHECKED)));for(auto* f:{w->python,w->idf,w->idfTools})lv_obj_set_hidden(lv_obj_get_parent(f),w->current!=1);for(auto* f:{w->config,w->library,w->fqbn})lv_obj_set_hidden(lv_obj_get_parent(f),w->current!=0);
}
void Start(Window* w,bool flash,bool probe=false,bool run=false){try{
    if(w->job&&!w->job->work->done.load())return;auto options=Read(w);w->settings[w->current]=options;SaveSettings(w);
    if(!probe&&!run) {
        nlohmann::json document;std::ifstream file(options.project/"ui/project.json",std::ios::binary);
        if(file)document=nlohmann::json::parse(file);else if(w->context.document)document=w->context.document();
        if(document.is_object()&&!document.empty()){const auto errors=ScreenValidation::BlockingErrors(document);if(!errors.empty())throw std::runtime_error("Build blocked by project validation:\n"+errors);}
    }
    auto commands=probe?DeviceTools::ProbePlan(options):run?DeviceTools::RunPlan(options):DeviceTools::BuildPlan(options,flash);
    if(flash&&w->monitor){w->monitor.reset();lv_label_set_text(w->serialStatus,"Monitor disconnected to release the flash port");}
    w->job=std::make_unique<Job>();w->jobRevision=0;auto work=w->job->work;
    w->job->thread=std::thread([work,commands=std::move(commands)]{work->result=DeviceTools::Run(commands,work->cancel,[work](const std::string& text){work->Append(text);});if(!work->result.error.empty())work->Append("\n"+work->result.error+"\n");work->done.store(true,std::memory_order_release);});
    Status(w,probe?"Checking tool version...":run?"Running the built application...":flash?"Flashing the configured board...":"Building project... compiler output appears below.");lv_obj_set_hidden(w->progress,true);lv_obj_set_hidden(w->spinner,false);lv_label_set_text(w->phase,"Waiting for tool output (progress is not reported yet)");lv_tabview_set_active(w->tabs,1,LV_ANIM_OFF);RefreshButtons(w);
    }catch(const std::exception& e){Status(w,e.what());}}
void Connect(Window* w){try{auto o=Read(w);w->settings[w->current]=o;SaveSettings(w);w->monitor=std::make_unique<Job>();w->serialRevision=0;auto work=w->monitor->work;work->Append("Opening "+o.port+" at "+std::to_string(o.baud)+" baud (8N1)\n");w->monitor->thread=std::thread([work,port=o.port,baud=o.baud]{work->result=DeviceTools::Monitor(port,baud,work->cancel,[work](const std::string& text){work->Append(text);});if(!work->result.error.empty())work->Append("\n"+work->result.error+"\n");work->done.store(true,std::memory_order_release);});lv_label_set_text(w->serialStatus,"Serial monitor running; disconnect before changing port or baud");RefreshButtons(w);}catch(const std::exception& e){lv_label_set_text(w->serialStatus,e.what());}}
void Poll(lv_timer_t* timer){auto* w=static_cast<Window*>(lv_timer_get_user_data(timer));
    auto readLog=[](const std::unique_ptr<Job>& job,size_t& revision,lv_obj_t* label){if(!job)return;std::string text;{std::lock_guard<std::mutex> lock(job->work->mutex);if(revision==job->work->revision)return;revision=job->work->revision;text=job->work->log;}lv_label_set_text(label,text.c_str());lv_obj_scroll_to_y(lv_obj_get_parent(label),LV_COORD_MAX,LV_ANIM_OFF);};
    readLog(w->job,w->jobRevision,w->log);readLog(w->monitor,w->serialRevision,w->serialLog);
    if(w->job&&w->job->work->done.load(std::memory_order_acquire)){const auto& r=w->job->work->result;Status(w,r.cancelled?"Stopped. The tool and its child processes were terminated.":!r.error.empty()?r.error:r.exitCode?"Tool failed with exit code "+std::to_string(r.exitCode)+". See output below.":"Completed successfully.");lv_bar_set_value(w->progress,r.cancelled||r.exitCode||!r.error.empty()?0:100,LV_ANIM_OFF);lv_obj_set_hidden(w->progress,false);lv_obj_set_hidden(w->spinner,true);lv_label_set_text(w->phase,r.cancelled?"Stopped":r.exitCode||!r.error.empty()?"Failed":"Finished");}
    else if(w->job){std::string phase;std::optional<unsigned> percent;{std::lock_guard<std::mutex> lock(w->job->work->mutex);phase=w->job->work->progress.phase;percent=w->job->work->progress.percent;}
        lv_obj_set_hidden(w->progress,!percent.has_value());lv_obj_set_hidden(w->spinner,percent.has_value());if(percent)lv_bar_set_value(w->progress,*percent,LV_ANIM_OFF);
        lv_label_set_text(w->phase,(phase+(percent?" - "+std::to_string(*percent)+"% reported by tool":" - progress not reported by tool")).c_str());}
    if(w->monitor&&w->monitor->work->done.load(std::memory_order_acquire)){const auto& r=w->monitor->work->result;lv_label_set_text(w->serialStatus,r.error.empty()?"Monitor disconnected":r.error.c_str());}
    RefreshButtons(w);
}
lv_obj_t* Logs(lv_obj_t* parent){auto* panel=lv_obj_create(parent);lv_obj_set_size(panel,LV_PCT(100),220);lv_obj_set_style_bg_color(panel,lv_color_hex(EditorStyle::Input),0);auto* label=lv_label_create(panel);lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_MODE_WRAP);lv_label_set_text(label,"Output will appear here.");return label;}
}
void Close(){if(!active)return;auto* w=active;auto* properties=w->properties;try{w->settings[w->current]=Read(w);SaveSettings(w);}catch(...){}EditorWindow::ReleaseFocus(w->root);lv_obj_delete(w->root);if(!shutdown)if(auto* selected=properties->GetSelectedObject())if(lv_obj_is_in_widget_tree(selected))properties->SetSelectedObject(selected);}
void Shutdown(){shutdown=true;Close();shutdown=false;}
lv_obj_t* ActiveWindow(){return active?active->root:nullptr;}
bool Running(){return active&&((active->job&&!active->job->work->done.load())||(active->monitor&&!active->monitor->work->done.load()));}
void SetContext(Context context){configured=std::move(context);if(active)active->context=configured;}
void Open(PropertyWindow* properties){if(active){lv_obj_move_foreground(active->root);return;}auto* w=new Window;w->properties=properties;w->context=configured;for(size_t i=0;i<3;++i)w->settings[i].profile=DeviceTools::Profile(i);LoadSettings(w);for(auto& o:w->settings)o=DeviceTools::Detect(o,Workspace());
    w->root=EditorWindow::Create(lv_layer_top());active=w;lv_obj_set_size(w->root,(std::min)(1040,lv_display_get_horizontal_resolution(nullptr)-40),(std::min)(900,lv_display_get_vertical_resolution(nullptr)-60));lv_obj_center(w->root);EditorWindow::SetTitle(w->root,"Build, flash and serial monitor");EditorWindow::SetCloseHandler(w->root,[]{Close();});lv_obj_add_event_cb(w->root,[](lv_event_t* e){auto* w=static_cast<Window*>(lv_event_get_user_data(e));if(active==w)active=nullptr;delete w;},LV_EVENT_DELETE,w);
    auto* content=EditorWindow::Content(w->root);lv_obj_set_scrollable(content,false);lv_obj_set_style_pad_all(content,0,0);w->tabs=lv_tabview_create(content);lv_obj_set_size(w->tabs,LV_PCT(100),LV_PCT(100));lv_tabview_set_tab_bar_size(w->tabs,38);
    auto* body=lv_tabview_add_tab(w->tabs,"Configure");auto* output=lv_tabview_add_tab(w->tabs,"Build output");auto* serial=lv_tabview_add_tab(w->tabs,"Serial monitor");for(auto* panel:{body,output,serial}){lv_obj_set_flex_flow(panel,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(panel,14,0);lv_obj_set_style_pad_row(panel,8,0);}
    EditorStyle::Help(body,"Choose a generated project and toolchain. Build and Flash board use these settings. Closing this window stops tools and releases serial ports.");
    auto* targetsRow=Row(body);auto* targetLabel=lv_label_create(targetsRow);lv_label_set_text(targetLabel,"Project target");lv_obj_set_width(targetLabel,160);w->targetChoice=lv_dropdown_create(targetsRow);lv_obj_set_flex_grow(w->targetChoice,1);Focus(w,w->targetChoice);
    w->targetName=Field(w,body,"Target name","For example: Raspberry Pi desktop or ESP32 panel");
    auto* targetButtons=Row(body);Button(w,targetButtons,"Save target",[w]{CommitTarget(w,false);});Button(w,targetButtons,"Save as new",[w]{CommitTarget(w,true);});Button(w,targetButtons,"Remove target",[w]{CommitTarget(w,false,true);});
    auto* row=Row(body);auto* name=lv_label_create(row);lv_label_set_text(name,"Toolchain");w->profile=lv_dropdown_create(row);lv_dropdown_set_options(w->profile,"Arduino CLI (Nano ESP32)\nESP-IDF\nCMake (Linux / Windows)");lv_dropdown_set_selected(w->profile,w->current);lv_obj_set_flex_grow(w->profile,1);Focus(w,w->profile);
    Button(w,row,"Detect tools",[w]{try{w->settings[w->current]=DeviceTools::Detect(Read(w),Workspace());Show(w,w->settings[w->current]);SaveSettings(w);Status(w,"Detection complete. Verify the project, board and port, then check the tool version.");}catch(const std::exception& e){Status(w,e.what());}},130);
    w->project=Field(w,body,"Project folder","Generated ESP-IDF project, Arduino sketch folder, or CMake source",true,true);
    Button(w,body,"Use project settings",[w]{try{const auto options=DeviceTargets::Infer(std::filesystem::u8path(lv_textarea_get_text(w->project)),{},Read(w));w->current=unsigned(options.profile);w->settings[w->current]=options;lv_dropdown_set_selected(w->profile,w->current);Show(w,options);lv_textarea_set_text(w->targetName,options.project.filename().u8string().c_str());Status(w,"Detected the project's toolchain and run command. Review settings, then Build.");RefreshButtons(w);}catch(const std::exception& error){Status(w,error.what());}},190);
    w->build=Field(w,body,"Build folder","Blank uses <project>/build",true,true);
    w->tool=Field(w,body,"CLI executable","arduino-cli or cmake; ESP-IDF uses Python below",true);
    w->python=Field(w,body,"ESP-IDF Python","IDF virtual environment python executable",true);
    w->idf=Field(w,body,"ESP-IDF root","Folder containing tools/idf.py",true,true);
    w->idfTools=Field(w,body,"IDF tools folder","IDF_TOOLS_PATH (for example C:/Espressif)",true,true);w->constraints=lv_checkbox_create(body);lv_checkbox_set_text(w->constraints,"Check ESP-IDF Python dependency constraints (leave off for the validated local SDK snapshot)");Focus(w,w->constraints);
    w->toolPath=Field(w,body,"Extra tool PATH","Compiler / Ninja folders; separate with ; on Windows or : on Linux");
    w->config=Field(w,body,"Arduino CLI config","Optional arduino-cli.yaml",true);
    w->library=Field(w,body,"Arduino library","Optional; generated LVGLProtobuf library is detected automatically",true,true);w->fqbn=Field(w,body,"Arduino board FQBN","arduino:esp32:nano_nora");
    w->port=Field(w,body,"Serial port","COM3 or /dev/ttyACM0");
    auto* portsRow=Row(body);Button(w,portsRow,"Find ports",[w]{const auto ports=DeviceTools::SerialPorts();std::string text="Detected ports: ";for(const auto& p:ports){if(text.back()!=' ')text+=", ";text+=p;}if(ports.empty())text+="none";else if(std::string(lv_textarea_get_text(w->port)).empty())lv_textarea_set_text(w->port,ports.front().c_str());Status(w,text);});
    w->baud=Field(w,body,"Serial / flash baud","115200");w->jobs=Field(w,body,"Build workers","4");
    w->wsl=lv_checkbox_create(body);lv_checkbox_set_text(w->wsl,"Build / run this Linux CMake project through WSL");Focus(w,w->wsl);
#ifndef _WIN32
    lv_obj_set_disabled(w->wsl,true);
#endif
    w->runProgram=Field(w,body,"Run executable","Detected for generated Linux projects; full path to built program",true);
    lv_obj_add_event_cb(w->targetChoice,[](lv_event_t* event){auto* w=static_cast<Window*>(lv_event_get_user_data(event));try{SelectTarget(w);RefreshButtons(w);}catch(const std::exception& error){Status(w,error.what());}},LV_EVENT_VALUE_CHANGED,w);
    lv_obj_add_event_cb(w->profile,[](lv_event_t* e){auto* w=static_cast<Window*>(lv_event_get_user_data(e));try{w->settings[w->current]=Read(w);}catch(...){}w->current=lv_dropdown_get_selected(w->profile);Show(w,w->settings[w->current]);Status(w,"Profile selected. Set the generated project folder for this toolchain.");RefreshButtons(w);},LV_EVENT_VALUE_CHANGED,w);
    auto* actions=Row(body);w->probeButton=Button(w,actions,"Tool version",[w]{Start(w,false,true);});w->buildButton=Button(w,actions,"Build",[w]{Start(w,false);},100);w->runButton=Button(w,actions,"Run built app",[w]{Start(w,false,false,true);},145);w->flashButton=Button(w,actions,"Flash board",[w]{Start(w,true);},120);w->stopButton=Button(w,actions,"Stop",[w]{if(w->job)w->job->work->cancel=true;},90);
    auto* outputActions=Row(output);Button(w,outputActions,"Configure",[w]{lv_tabview_set_active(w->tabs,0,LV_ANIM_OFF);});Button(w,outputActions,"Stop tool",[w]{if(w->job)w->job->work->cancel=true;});
    w->configStatus=lv_label_create(body);lv_obj_set_width(w->configStatus,LV_PCT(100));lv_label_set_long_mode(w->configStatus,LV_LABEL_LONG_MODE_WRAP);w->status=lv_label_create(output);lv_obj_set_width(w->status,LV_PCT(100));lv_label_set_long_mode(w->status,LV_LABEL_LONG_MODE_WRAP);Status(w,"Choose a project folder. Flash board uploads existing build output to the configured serial port.");w->phase=lv_label_create(output);lv_label_set_text(w->phase,"No tool running");w->spinner=lv_spinner_create(output);lv_obj_set_size(w->spinner,24,24);lv_obj_set_hidden(w->spinner,true);w->progress=lv_bar_create(output);lv_obj_set_size(w->progress,LV_PCT(100),6);w->log=Logs(output);lv_obj_set_flex_grow(lv_obj_get_parent(w->log),1);
    EditorStyle::Help(serial,"Port and baud are set on Configure. The monitor releases the port before flashing. Data is read in the background and output is limited to the most recent 128 KiB.");
    auto* serialActions=Row(serial);w->connectButton=Button(w,serialActions,"Connect monitor",[w]{Connect(w);},170);w->disconnectButton=Button(w,serialActions,"Disconnect",[w]{w->monitor.reset();lv_label_set_text(w->serialStatus,"Monitor disconnected");RefreshButtons(w);},130);Button(w,serialActions,"Clear output",[w]{if(w->monitor){std::lock_guard<std::mutex> lock(w->monitor->work->mutex);w->monitor->work->log.clear();++w->monitor->work->revision;}lv_label_set_text(w->serialLog,"");},130);
    w->serialStatus=lv_label_create(serial);lv_obj_set_width(w->serialStatus,LV_PCT(100));lv_label_set_long_mode(w->serialStatus,LV_LABEL_LONG_MODE_WRAP);lv_label_set_text(w->serialStatus,"Serial monitor disconnected. No hardware is accessed until Connect monitor or Flash board is selected.");w->serialLog=Logs(serial);lv_obj_set_flex_grow(lv_obj_get_parent(w->serialLog),1);
    Show(w,w->settings[w->current]);RefreshTargets(w);RefreshButtons(w);w->timer=lv_timer_create(Poll,100,w);FocusEditorControl(properties->GetKBGroup(),w->project);
}
void OpenProject(PropertyWindow* properties,const std::filesystem::path& project,const nlohmann::json& manifest){
    Open(properties);auto* w=active;if(!w)return;
    try{
        if(Running())throw std::runtime_error("Stop the current tool before switching build projects");
        auto options=DeviceTargets::Infer(project,manifest,w->settings[w->current]);std::string selected;
        if(w->context.document){const auto document=w->context.document();for(const auto& target:document.value("deviceTargets",nlohmann::json::array()))if(target.is_object()&&target.contains("options")){
            const auto saved=DeviceTargets::Restore(target.at("options"));if(saved.project.lexically_normal()==options.project.lexically_normal()){options=saved;selected=target.value("id",std::string{});break;}
        }}
        w->current=unsigned(options.profile);w->settings[w->current]=DeviceTools::Detect(options,Workspace());lv_dropdown_set_selected(w->profile,w->current);Show(w,w->settings[w->current]);RefreshTargets(w,selected);lv_textarea_set_text(w->targetName,project.filename().u8string().c_str());RefreshButtons(w);lv_tabview_set_active(w->tabs,0,LV_ANIM_OFF);Status(w,"Generated project is ready. Check the target, Build, then Run the app or Flash the board.");
    }catch(const std::exception& error){Status(w,error.what());}
}
}


