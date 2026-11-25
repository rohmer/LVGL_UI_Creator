#include "ProjectCreator.h"
#include "../Utils/ApplicationPaths.h"
#include "../Utils/ExternalTool.h"
#include "../../CodeGeneration/CCodeGenerator.h"
#include "../../CodeGeneration/ProjectAssets.h"
#include "../../CodeGeneration/Targets/Target.h"
#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <Serialization/Protobuf.h>
#include <Serialization/ProjectDesign.h>
#include <Serialization/ProjectScreens.h>
#include <Serialization/ProjectTries.h>
#include <chrono>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif
namespace ProjectCreation {
namespace {
std::string Read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read project-creator result");return {(std::istreambuf_iterator<char>(in)),{}};}
json WithBundledTemplates(json catalog,const json& bundled){
    if(!catalog.is_array())throw std::invalid_argument("Invalid project catalog");
    for(const auto& item:bundled)if(item.contains("uicreatorTemplate")){
        const auto id=item.at("uicreatorTemplate");
        catalog.erase(std::remove_if(catalog.begin(),catalog.end(),[&](const json& entry){return entry.value("uicreatorTemplate",json{})==id;}),catalog.end());
        catalog.push_back(item);
    }
    return catalog;
}
std::filesystem::path ExecutableFolder(){
    return ApplicationPaths::ResourceDirectory();
}
#ifndef _WIN32
bool Launch(const std::string& arg){auto child=fork();if(child<0)return false;if(child==0){
#ifdef __APPLE__
    execlp("open","open",arg.c_str(),(char*)nullptr);
#else
    execlp("xdg-open","xdg-open",arg.c_str(),(char*)nullptr);
#endif
    _exit(127);
}return true;}
#endif
std::string EscapeUrl(const std::string& input){const char* hex="0123456789ABCDEF";std::string out;for(unsigned char c:input){if(std::isalnum(c)||c=='/'||c==':'||c=='-'||c=='_'||c=='.')out+=char(c);else {out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;}
}
std::filesystem::path RuntimeDirectory(){return ExecutableFolder()/"project-creator";}
std::filesystem::path DefaultCache(){
#ifdef _WIN32
    wchar_t path[32768];auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);if(count && count<32768)return std::filesystem::path(path)/"UICreator"/"project-creator";
#else
#ifdef __APPLE__
    if(const char* home=std::getenv("HOME"))return std::filesystem::path(home)/"Library/Caches/LVGL UI Creator/project-creator";
#endif
    if(const char* cache=std::getenv("XDG_CACHE_HOME"))return std::filesystem::path(cache)/"UICreator"/"project-creator";
    if(const char* home=std::getenv("HOME"))return std::filesystem::path(home)/".cache"/"UICreator"/"project-creator";
#endif
    return std::filesystem::temp_directory_path()/"UICreator"/"project-creator-cache";
}
json BundledCatalog(){
    const auto bundled=json::parse(Read(RuntimeDirectory()/"catalog.json"));auto cache=DefaultCache()/"catalog.json";
    try {if(std::filesystem::exists(cache))return WithBundledTemplates(json::parse(Read(cache)),bundled);}catch(const std::exception&){}
    return bundled;
}
Result Run(json request,const std::atomic<bool>* cancel){
    Result result;std::filesystem::path workspace;bool owns=false;
    try {
        if(cancel && cancel->load())throw std::runtime_error("Project creation cancelled");
        auto operation=request.value("operation",std::string("create"));
        auto parent=operation=="create"?std::filesystem::absolute(std::filesystem::u8path(request.at("destination").get<std::string>())).parent_path():std::filesystem::temp_directory_path();
        std::filesystem::create_directories(parent);
        static std::atomic<uint64_t> sequence{0};
        workspace=parent/(".uicreator-job-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(sequence++));
        owns=std::filesystem::create_directory(workspace);if(!owns)throw std::runtime_error("Cannot create project workspace");
        request["workspace"]=workspace.u8string();if(!request.contains("cache"))request["cache"]=DefaultCache().u8string();
        const auto version=request.value("lvglVersion",std::string("template"));
        const bool linux=operation=="create" && request.at("manifest").value("uicreatorTemplate",std::string{})=="raspberry-pi-linux";
        const auto templateId=operation=="create"?request.at("manifest").value("uicreatorTemplate",std::string{}):std::string{};
        const bool pico=templateId=="raspberry-pi-pico"||templateId=="raspberry-pi-pico2";
        const bool native=linux||pico;
        if(native){
            if(version!="template" && version!="9.6.0")throw std::invalid_argument("Built-in Raspberry Pi projects use bundled LVGL 9.6.0");
            request[pico?"picoToolkitArchive":"linuxToolkitArchive"]=(RuntimeDirectory()/(pico?"pico-toolkit.zip":"linux-toolkit.zip")).u8string();
        }
        if(version=="9.6.0" && !request.contains("lvglArchive"))request["lvglArchive"]=(RuntimeDirectory()/"lvgl-9.6.0.zip").u8string();
        auto format=request.value("format",std::string("json"));
        if(operation=="create" && (format!="json" || native)) {
            auto nativeVersion=native?std::string("9.6.0"):version=="template"?request.at("manifest").value("lvglVersion",std::string{}):version;
            auto end=nativeVersion.find('.',nativeVersion.find('.')+1);auto target=nativeVersion.substr(0,end);
            auto source=request.value("sourceDirectory",std::string{});auto project=request.at("project");
            auto ui=workspace/"ui";std::filesystem::create_directory(ui);
            if(format=="c") {
                CCodeGenerator generator;generator.SetTargetVersion(target);generator.SetSourceDirectory(source);
                if(!generator.LoadProject(project)||!generator.ExportProject(ui.u8string()))throw std::runtime_error(generator.GetLastError());
            } else if(format!="protobuf" && format!="json")throw std::invalid_argument("Unknown UI output format");
            if(format=="protobuf" || native) {
                project=Serialization::ProjectScreens::Device(project);
                Serialization::ProjectDesign::Materialize(project,true);
                Serialization::ProjectTries::PruneUnused(project);
                CGeneration::ValidateTarget(target);CGeneration::EmbedLocalImages(project,std::filesystem::u8path(source));project["runtimeTarget"]=target;
                auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",project);
                std::ofstream out(ui/"project.pb",std::ios::binary);out.write((const char*)bytes.data(),bytes.size());out.close();if(!out)throw std::runtime_error("Cannot write project Protobuf");
            }
            request["uiSourceFolder"]=ui.u8string();
        }
        {std::ofstream out(workspace/"request.json",std::ios::binary);out<<request.dump();if(!out)throw std::runtime_error("Cannot write project request");}
        auto executable=RuntimeDirectory()/
#ifdef _WIN32
            "UICreatorProjectTool.exe";
#else
            "UICreatorProjectTool";
#endif
        if(!std::filesystem::exists(executable))throw std::runtime_error("Bundled project creator is missing; rebuild the application assets");
        // Offline Linux/Pico projects extract, verify and copy thousands of
        // bundled source files before publication, especially on Windows.
        const auto timeout=native?std::chrono::minutes(10):std::chrono::minutes(3);
        auto code=ExternalTool::Run(executable,"--request",workspace/"request.json",workspace/"log.txt",cancel,timeout);
        if(code!=0)throw std::runtime_error(cancel && cancel->load()?"Project creation cancelled":Read(workspace/"log.txt"));
        result.data=json::parse(Read(workspace/"result.json"));result.success=true;result.message=operation=="create"?"Project created":operation=="preview"?"Template image loaded":"Catalog updated";
    }catch(const std::exception& e){result.message=e.what();}
    if(owns){std::error_code ignored;std::filesystem::remove_all(workspace,ignored);}return result;
}
bool OpenUrl(const std::string& url){if(url.rfind("https://",0)!=0 && url.rfind("http://",0)!=0)return false;
#ifdef _WIN32
    auto path=std::filesystem::u8path(url).wstring();return (INT_PTR)ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL)>32;
#else
    return Launch(url);
#endif
}
bool OpenFolder(const std::filesystem::path& folder,bool vscode){if(!std::filesystem::is_directory(folder))return false;
    if(vscode){auto uri="vscode://file/"+EscapeUrl(std::filesystem::absolute(folder).generic_u8string());
#ifdef _WIN32
        auto text=std::filesystem::u8path(uri).wstring();return (INT_PTR)ShellExecuteW(nullptr,L"open",text.c_str(),nullptr,nullptr,SW_SHOWNORMAL)>32;
#else
        return Launch(uri);
#endif
    }
#ifdef _WIN32
    return (INT_PTR)ShellExecuteW(nullptr,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL)>32;
#else
    return Launch(folder.u8string());
#endif
}
}
