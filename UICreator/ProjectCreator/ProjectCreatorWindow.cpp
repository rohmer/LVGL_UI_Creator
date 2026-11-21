#include "ProjectCreatorWindow.h"
#ifndef _WIN32
#include "../Utils/NativeFileDialogUI.h"
#endif
#include "../Devices/DeviceWindow.h"
#include "../Validation/ScreenValidation.h"
#include "../Windows/PropertyWindow.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include "../Images/ImageConverter.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#endif
namespace ProjectCreatorWindow {
using json=ProjectCreation::json;
namespace {
struct Job {std::atomic<bool> done{false},cancel{false};ProjectCreation::Result result;bool refresh=false,preview=false;std::thread worker;};
struct Window {
    PropertyWindow* properties;std::function<json()> project;std::string sourceDirectory;
    lv_obj_t *root=nullptr,*search=nullptr,*host=nullptr,*environment=nullptr,*vendor=nullptr,*list=nullptr,*details=nullptr,*branch=nullptr,*version=nullptr,*settings=nullptr,*destination=nullptr,*lvglPath=nullptr,*format=nullptr,*status=nullptr,*create=nullptr,*preview=nullptr,*logo=nullptr,*purchases=nullptr,*note=nullptr;
    json catalog=json::array();std::vector<size_t> filtered;std::vector<std::string> environments,vendors,logoUrls;std::map<size_t,lv_obj_t*> options;
    std::shared_ptr<Job> job;lv_timer_t* timer=nullptr;std::filesystem::path created;json createdManifest;lv_obj_t* buildCreated=nullptr;
    ~Window(){if(job){job->cancel=true;if(job->worker.joinable())job->worker.join();}if(timer)lv_timer_delete(timer);}
};
lv_obj_t* active=nullptr;
std::string Text(const json& j){return j.is_string()?j.get<std::string>():j.is_null()?std::string{}:j.dump();}
std::string Lower(std::string s){for(char& c:s)c=(char)std::tolower((unsigned char)c);return s;}
std::string Choices(const std::vector<std::string>& values){std::string result;for(auto s:values){std::replace(s.begin(),s.end(),'\n',' ');if(!result.empty())result+='\n';result+=s;}return result;}
void Status(Window* w,const std::string& text){lv_label_set_text(w->status,text.c_str());}
lv_obj_t* Row(lv_obj_t* parent){auto* r=lv_obj_create(parent);lv_obj_set_size(r,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(r,LV_FLEX_FLOW_ROW);lv_obj_set_flex_align(r,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);lv_obj_set_style_pad_all(r,0,LV_PART_MAIN);lv_obj_set_style_border_width(r,0,LV_PART_MAIN);lv_obj_set_scrollable(r,false);return r;}
lv_obj_t* Label(lv_obj_t* parent,const std::string& text){auto* label=lv_label_create(parent);lv_label_set_text(label,text.c_str());return label;}
lv_obj_t* Button(lv_obj_t* parent,const char* text,std::function<void()> fn){auto* b=lv_button_create(parent);lv_obj_set_height(b,36);auto* label=Label(b,text);lv_obj_center(label);auto* callback=new std::function<void()>(std::move(fn));lv_obj_add_event_cb(b,[](lv_event_t* e){auto* fn=static_cast<std::function<void()>*>(lv_event_get_user_data(e));if(lv_event_get_code(e)==LV_EVENT_DELETE)delete fn;else if(lv_event_get_code(e)==LV_EVENT_CLICKED){auto action=*fn;action();}},LV_EVENT_ALL,callback);return b;}
void Focus(Window* w,lv_obj_t* o){BindEditorControl(w->properties->GetKBGroup(),o);}
lv_obj_t* Input(Window* w,lv_obj_t* parent,const char* caption,const std::string& initial){auto* row=Row(parent);auto* label=Label(row,caption);lv_obj_set_width(label,155);auto* o=lv_textarea_create(row);lv_textarea_set_one_line(o,true);lv_textarea_set_text(o,initial.c_str());lv_obj_set_height(o,36);lv_obj_set_flex_grow(o,1);Focus(w,o);return o;}
lv_obj_t* Dropdown(Window* w,lv_obj_t* parent,const char* caption,const std::string& choices){auto* row=Row(parent);auto* label=Label(row,caption);lv_obj_set_width(label,155);auto* o=lv_dropdown_create(row);lv_dropdown_set_options(o,choices.c_str());lv_obj_set_flex_grow(o,1);Focus(w,o);return o;}
const json* Selected(Window* w){auto index=lv_dropdown_get_selected(w->list);return index<w->filtered.size()?&w->catalog[w->filtered[index]]:nullptr;}
void Select(Window* w){
    if(w->job && w->job->preview)w->job->cancel=true;
    w->options.clear();lv_obj_clean(w->settings);auto* selected=Selected(w);if(!selected){lv_label_set_text(w->details,"No matching templates");lv_obj_set_disabled(w->create,true);return;}
    const auto& m=*selected;std::string text=Text(m.value("description",json{}));
    const bool linux=m.value("uicreatorTemplate",std::string{})=="raspberry-pi-linux";
    const auto oldVersion=lv_dropdown_get_selected(w->version);
    const unsigned versionCount=linux?2:7;
    if(lv_dropdown_get_option_count(w->version)!=versionCount){
        lv_dropdown_set_options(w->version,linux?"From template (9.6.0)\n9.6.0 (bundled)":"From template\n9.6.0 (bundled)\n9.5.0\n9.3.0\n9.2.2\n9.1.0\n8.3.11");
        lv_dropdown_set_selected(w->version,(std::min)(unsigned(oldVersion),versionCount-1));
    }
    lv_obj_set_disabled(w->branch,linux);lv_obj_set_disabled(w->lvglPath,linux);
    if(linux)lv_textarea_set_text(w->lvglPath,"");
    lv_label_set_text(w->note,linux?
        "Creates an offline Linux CMake build with the current screen, Advanced Keyboard and only the required LVGL widgets. A runnable Protobuf is included in every output format; C export also includes generated source.":
        "Use the template version to preserve its tested SDK setup. Choose a supported LVGL version to export C or Protobuf. Settings below come from the template manifest.");
    text+="\n\nMaintainer: "+Text(m.value("maintainer",json{}))+"\nHost OS: "+Text(m.value("hostOperatingsystem",json{}))+"\nEnvironment: "+Text(m.value("environment",json{}));
    text+="\nTemplate LVGL: "+Text(m.value("lvglVersion",json("not specified")))+"\nUpdated: "+Text(m.value("lastUpdated",json("not specified")));
    if(m.contains("hardware"))text+="\nHardware: "+m.at("hardware").dump(2);
    if(m.contains("getStartedInstructions"))text+="\n\nGetting started\n"+Text(m["getStartedInstructions"]);
    if(m.contains("buy_now_links"))text+="\nPurchase links: "+m["buy_now_links"].dump();
    lv_label_set_text(w->details,text.c_str());
    w->logoUrls.clear();if(m.contains("image") && m["image"].is_string())w->logoUrls.push_back(m["image"]);
    for(auto& url:m.value("logos",json::array()))if(std::find(w->logoUrls.begin(),w->logoUrls.end(),Text(url))==w->logoUrls.end())w->logoUrls.push_back(Text(url));
    std::vector<std::string> logos;for(size_t i=0;i<w->logoUrls.size();++i)logos.push_back("Image / logo "+std::to_string(i+1));lv_dropdown_set_options(w->logo,logos.empty()?"No image":Choices(logos).c_str());
    lv_obj_set_hidden(w->preview,true);lv_obj_clean(w->purchases);size_t purchase=0;
    for(auto& url:m.value("buy_now_links",json::array())){auto link=Text(url);auto caption="Vendor page "+std::to_string(++purchase);Button(w->purchases,caption.c_str(),[link]{ProjectCreation::OpenUrl(link);});}

    std::vector<std::string> branches{"Default branch"};for(auto& b:m.value("branches",json::array()))branches.push_back(Text(b));lv_dropdown_set_options(w->branch,Choices(branches).c_str());
    auto settings=m.value("settings",m.value("Settings",json::array()));
    for(size_t i=0;i<settings.size();++i){auto& s=settings[i];if(s.value("type",std::string{})!="dropdown")continue;std::vector<std::string> names;uint32_t def=0;
        for(auto& option:s.at("options")){names.push_back(Text(option.at("name")));if(Lower(Text(option.value("default",json(false))))=="true")def=(uint32_t)names.size()-1;}
        auto* choice=Dropdown(w,w->settings,Text(s.value("label",json("Setting"))).c_str(),Choices(names));lv_dropdown_set_selected(choice,def);w->options[i]=choice;
        for(auto& option:s.at("options"))if(option.contains("help")){auto* help=Label(w->settings,Text(option["name"])+": "+Text(option["help"]));lv_obj_set_width(help,LV_PCT(100));}
    }
    lv_obj_set_disabled(w->create,w->job && !w->job->done.load());
}
void Filter(Window* w){
    auto query=Lower(lv_textarea_get_text(w->search));auto host=lv_dropdown_get_selected(w->host);auto env=lv_dropdown_get_selected(w->environment),vendor=lv_dropdown_get_selected(w->vendor);w->filtered.clear();std::vector<std::string> names;
    const char* systems[]={"","Windows","Linux","MacOS"};
    for(size_t i=0;i<w->catalog.size();++i){const auto& m=w->catalog[i];if(!query.empty() && Lower(m.dump()).find(query)==std::string::npos)continue;
        auto hosts=m.value("hostOperatingsystem",json::array());if(host && std::find(hosts.begin(),hosts.end(),systems[host])==hosts.end())continue;
        if(env && m.value("environment",json::array()).dump().find(w->environments[env])==std::string::npos)continue;
        if(vendor && m.value("hardware",json::object()).dump().find(w->vendors[vendor])==std::string::npos)continue;
        w->filtered.push_back(i);names.push_back(Text(m.at("name")));}
    lv_dropdown_set_options(w->list,names.empty()?"No matches":Choices(names).c_str());Select(w);
}
void Catalog(Window* w,json catalog){
    if(!catalog.is_array())throw std::runtime_error("Invalid project catalog");w->catalog=std::move(catalog);w->environments={"All environments"};w->vendors={"All vendors"};
    for(auto& m:w->catalog){for(auto& e:m.value("environment",json::array()))w->environments.push_back(Text(e));if(m.contains("hardware") && m["hardware"].is_object())for(const char* key:{"chipVendor","manufacturer"})if(m["hardware"].contains(key))w->vendors.push_back(Text(m["hardware"][key]));}
    auto unique=[](std::vector<std::string>& values){std::sort(values.begin()+1,values.end());values.erase(std::unique(values.begin(),values.end()),values.end());};unique(w->environments);unique(w->vendors);
    lv_dropdown_set_options(w->environment,Choices(w->environments).c_str());lv_dropdown_set_options(w->vendor,Choices(w->vendors).c_str());Filter(w);
}
void Start(Window* w,bool refresh){
    if(w->job && !w->job->done.load())return;
    try {json request={{"operation",refresh?"refresh":"create"}};
        if(!refresh){auto* selected=Selected(w);if(!selected)throw std::runtime_error("Select a template");request["manifest"]=*selected;
            request["destination"]=lv_textarea_get_text(w->destination);if(request["destination"].get<std::string>().empty())throw std::runtime_error("Choose a destination folder");
            auto branch=lv_dropdown_get_selected(w->branch);request["branch"]=branch?selected->at("branches").at(branch-1):json("");
            static const char* versions[]={"template","9.6.0","9.5.0","9.3.0","9.2.2","9.1.0","8.3.11"};request["lvglVersion"]=versions[lv_dropdown_get_selected(w->version)];
            static const char* formats[]={"json","c","protobuf"};request["format"]=formats[lv_dropdown_get_selected(w->format)];request["project"]=w->project();const auto errors=ScreenValidation::BlockingErrors(request["project"],w->properties->GetObjectTree()->GetObjects());if(!errors.empty())throw std::runtime_error("Project generation blocked by validation:\n"+errors);request["sourceDirectory"]=w->sourceDirectory;
            request["lvglPath"]=lv_textarea_get_text(w->lvglPath);request["settings"]=json::object();auto settings=selected->value("settings",selected->value("Settings",json::array()));
            for(auto [index,control]:w->options)request["settings"][std::to_string(index)]=settings[index]["options"][lv_dropdown_get_selected(control)]["value"];
        }
        w->job=std::make_shared<Job>();w->job->refresh=refresh;auto job=w->job;lv_obj_set_disabled(w->create,true);Status(w,refresh?"Refreshing template catalog...":"Downloading and configuring project... You can cancel this operation.");
        job->worker=std::thread([job,request]{job->result=ProjectCreation::Run(request,&job->cancel);job->done=true;});
    }catch(const std::exception& e){Status(w,e.what());}
}
void Preview(Window* w) {
    if(w->job && !w->job->done.load())return;auto index=lv_dropdown_get_selected(w->logo);if(index>=w->logoUrls.size()){Status(w,"This template has no image");return;}
    w->job=std::make_shared<Job>();w->job->preview=true;auto job=w->job;auto url=w->logoUrls[index];Status(w,"Loading template image...");
    job->worker=std::thread([job,url]{job->result=ProjectCreation::Run({{"operation","preview"},{"url",url}},&job->cancel);
        if(job->result.success){auto image=ImageConversion::Convert({std::filesystem::u8path(job->result.data.at("path").get<std::string>())},ImageConversion::Defaults(),{},&job->cancel);
            if(image.success)job->result.data["preview"]=image.assets.at(0);else {job->result.success=false;job->result.message=image.message;}}
        job->done=true;
    });
}
void Poll(lv_timer_t* timer){auto* w=static_cast<Window*>(lv_timer_get_user_data(timer));if(!w->job || !w->job->done.load())return;auto job=w->job;if(job->worker.joinable())job->worker.join();w->job.reset();
    if(job->preview && job->cancel.load()){job->result.success=false;job->result.message="Image request cancelled";}
    if(job->result.success){if(job->refresh)Catalog(w,job->result.data.at("catalog"));else if(job->preview){Serialization::ImageAsset::Restore(w->preview,job->result.data.at("preview"));lv_image_set_inner_align(w->preview,LV_IMAGE_ALIGN_CONTAIN);lv_obj_set_hidden(w->preview,false);}else {w->created=std::filesystem::u8path(job->result.data.at("destination").get<std::string>());w->createdManifest=job->result.data.value("template",json::object());lv_obj_set_disabled(w->buildCreated,false);}}
    lv_obj_set_disabled(w->create,!Selected(w));Status(w,job->result.message+(job->result.success && !job->refresh && !job->preview?"\n"+w->created.u8string():std::string{}));
}
void Browse(Window* w){
#ifdef _WIN32
    auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);IFileOpenDialog* dialog=nullptr;
    if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))){DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);dialog->SetTitle(L"Choose parent folder for the new LVGL project");if(SUCCEEDED(dialog->Show(GetActiveWindow()))){IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){auto target=std::filesystem::path(path)/"lvgl_project";lv_textarea_set_text(w->destination,target.u8string().c_str());CoTaskMemFree(path);}item->Release();}}dialog->Release();}if(SUCCEEDED(initialized))CoUninitialize();
#else
    const auto parent=NativeFileDialogUI::PickOne(false,true,L"Choose parent folder for the new LVGL project");
    if(!parent.empty())lv_textarea_set_text(w->destination,(parent/"lvgl_project").u8string().c_str());
#endif
}
}
void Close(){if(active){auto* object=active;active=nullptr;lv_obj_delete(object);}}
void Open(PropertyWindow* properties,std::function<json()> project,const std::string& sourceDirectory,const std::string& templateId){
    if(active){lv_obj_move_foreground(active);return;}auto* w=new Window;w->properties=properties;w->project=std::move(project);w->sourceDirectory=sourceDirectory;
    w->root=EditorWindow::Create(lv_layer_top());EditorWindow::SetCloseHandler(w->root,[]{Close();});active=w->root;EditorWindow::SetTitle(w->root,"Create LVGL firmware project");
    lv_obj_set_size(w->root,(std::min)(900,lv_display_get_horizontal_resolution(nullptr)-40),(std::min)(850,lv_display_get_vertical_resolution(nullptr)-40));lv_obj_center(w->root);
    lv_obj_add_event_cb(w->root,[](lv_event_t* e){if(active==lv_event_get_target_obj(e))active=nullptr;delete static_cast<Window*>(lv_event_get_user_data(e));},LV_EVENT_DELETE,w);
    auto* content=EditorWindow::Content(w->root);lv_obj_set_flex_flow(content,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(content,16,LV_PART_MAIN);lv_obj_set_style_pad_row(content,10,LV_PART_MAIN);
    w->search=Input(w,content,"Search templates","");w->host=Dropdown(w,content,"Host operating system","All hosts\nWindows\nLinux\nMacOS");w->environment=Dropdown(w,content,"Environment","All environments");w->vendor=Dropdown(w,content,"Chip / board vendor","All vendors");w->list=Dropdown(w,content,"Project template","Loading...");
    w->logo=Dropdown(w,content,"Template image","No image");auto* imageButtons=Row(content);Button(imageButtons,"Load image / logo",[w]{Preview(w);});
    w->preview=lv_image_create(content);lv_obj_set_size(w->preview,320,180);lv_obj_set_hidden(w->preview,true);
    w->details=Label(content,"");lv_obj_set_width(w->details,LV_PCT(100));w->purchases=Row(content);
    auto* links=Row(content);Button(links,"Repository",[w]{if(auto* m=Selected(w))ProjectCreation::OpenUrl(m->at("urlToClone"));});Button(links,"Project Creator docs",[]{ProjectCreation::OpenUrl("https://github.com/lvgl/lvgl_project_creator");});Button(links,"Refresh catalog",[w]{Start(w,true);});
    w->branch=Dropdown(w,content,"Template branch","Default branch");w->version=Dropdown(w,content,"LVGL source version","From template\n9.6.0 (bundled)\n9.5.0\n9.3.0\n9.2.2\n9.1.0\n8.3.11");
    w->lvglPath=Input(w,content,"LVGL folder (auto)","");w->format=Dropdown(w,content,"Include current UI as","Project JSON\nC code + project JSON\nProtobuf + project JSON");
    w->note=Label(content,"Use the template version to preserve its tested SDK setup. Choose a supported LVGL version to export C or Protobuf. Settings below come from the template manifest.");lv_obj_set_width(w->note,LV_PCT(100));
    w->settings=lv_obj_create(content);lv_obj_set_size(w->settings,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(w->settings,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(w->settings,0,LV_PART_MAIN);lv_obj_set_style_border_width(w->settings,0,LV_PART_MAIN);
    w->destination=Input(w,content,"Destination folder",(std::filesystem::current_path()/"lvgl_project").u8string());auto* browse=Row(content);Button(browse,"Choose folder...",[w]{Browse(w);});
    w->status=Label(content,"Catalog available offline. The first download needs an internet connection; subsequent creation can reuse cached sources.");lv_obj_set_width(w->status,LV_PCT(100));
    auto* buttons=Row(content);w->create=Button(buttons,"Create project",[w]{Start(w,false);});Button(buttons,"Cancel operation",[w]{if(w->job){w->job->cancel=true;Status(w,"Cancelling...");}});Button(buttons,"Close",[]{Close();});
    auto* opened=Row(content);w->buildCreated=Button(opened,"Build this project",[w]{if(w->created.empty()){Status(w,"Create a project before building it.");return;}DeviceWindow::OpenProject(w->properties,w->created,w->createdManifest);});lv_obj_set_disabled(w->buildCreated,true);Button(opened,"Open folder",[w]{if(!ProjectCreation::OpenFolder(w->created))Status(w,"Create a project first, then open its folder.");});Button(opened,"Open in VS Code",[w]{if(!ProjectCreation::OpenFolder(w->created,true))Status(w,"Install VS Code or open the generated folder manually.");});
    for(auto* control:{w->search,w->host,w->environment,w->vendor})lv_obj_add_event_cb(control,[](lv_event_t* e){Filter(static_cast<Window*>(lv_event_get_user_data(e)));},LV_EVENT_VALUE_CHANGED,w);
    lv_obj_add_event_cb(w->list,[](lv_event_t* e){Select(static_cast<Window*>(lv_event_get_user_data(e)));},LV_EVENT_VALUE_CHANGED,w);
    try {
        Catalog(w,ProjectCreation::BundledCatalog());
        if(!templateId.empty())for(const auto& manifest:w->catalog)if(manifest.value("uicreatorTemplate",std::string{})==templateId){
            const auto vendor=manifest.value("hardware",json::object()).value("chipVendor",std::string{});
            const auto found=std::find(w->vendors.begin(),w->vendors.end(),vendor);
            if(found!=w->vendors.end())lv_dropdown_set_selected(w->vendor,static_cast<uint32_t>(std::distance(w->vendors.begin(),found)));
            Filter(w);
            for(size_t i=0;i<w->filtered.size();++i)if(w->catalog[w->filtered[i]].value("uicreatorTemplate",std::string{})==templateId){lv_dropdown_set_selected(w->list,static_cast<uint32_t>(i));Select(w);break;}
            break;
        }
    }catch(const std::exception& e){Status(w,e.what());lv_obj_set_disabled(w->create,true);}
    w->timer=lv_timer_create(Poll,100,w);
}
}
