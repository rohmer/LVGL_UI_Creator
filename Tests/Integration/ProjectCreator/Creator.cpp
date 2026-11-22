#include "ProjectCreator.h"
#include "lvgl_runtime.h"
#include <Serialization/Protobuf.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
using json=nlohmann::json;
#define CHECK(x) do {if(!(x))throw std::runtime_error(std::string("Line ")+std::to_string(__LINE__)+": " #x);}while(0)
namespace {
json ReadJson(const std::filesystem::path& path) {std::ifstream in(path);CHECK(in);return json::parse(in);}
std::string ReadText(const std::filesystem::path& path) {std::ifstream in(path,std::ios::binary);CHECK(in);return {(std::istreambuf_iterator<char>(in)),{}};}
Serialization::Binary ReadBinary(const std::filesystem::path& path) {std::ifstream in(path,std::ios::binary);CHECK(in);return {(std::istreambuf_iterator<char>(in)),{}};}
json Widget(int oid,const char* kind,int x,int y,int width,int height) {
    return {{"oid",oid},{"par",-1},{"base",{{"name",std::string(kind)+std::to_string(oid)},{"coords",{{"x",x},{"y",y},{"width",width},{"height",height}}}}},{kind,json::object()}};
}
json PiProject() {
    auto entry=Widget(0,"textarea",10,10,600,70);entry["textarea"]={{"text","Linux keyboard"},{"maxLength",64}};
    auto keyboard=Widget(1,"advancedKeyboard",10,120,600,240);keyboard["advancedKeyboard"]={{"language","en-US"},{"trieId","builtin-en-us"},{"targetOid",0},{"autoFocus",true},{"hideOnDefocus",false},{"autocorrect",true},{"autocorrectThreshold",0.3}};
    auto count=Widget(2,"text",240,86,100,28);count["text"]="0";
    auto button=Widget(3,"button",16,85,180,32);button["button"]["text"]="Increment";
    button["base"]["events"]=json::array({{{"event","clicked"},{"action","increment_variable"},{"variable","count"},{"value",1}}});
    return {{"formatVersion",2},{"lvglVersion","9.6.0"},{"canvas",{{"width",640},{"height",400}}},
        {"obj",json::array({entry,keyboard,count,button})},
        {"variables",json::array({{{"var",{{"name","count"},{"dataType",1},{"default",0},{"history",4},{"oid",2},{"property","text"}}}},
            {{"var",{{"name","message"},{"dataType",0},{"default","Linux keyboard"},{"history",4},{"oid",0},{"property","text"}}}}})}};
}
struct CacheEnvironment {
#ifdef _WIN32
    const char* name="LOCALAPPDATA";
#else
    const char* name="XDG_CACHE_HOME";
#endif
    std::string old;bool present=false;
    explicit CacheEnvironment(const std::filesystem::path& directory) {
        if(const auto* value=std::getenv(name)){present=true;old=value;}
#ifdef _WIN32
        CHECK(_putenv_s(name,directory.u8string().c_str())==0);
#else
        CHECK(setenv(name,directory.u8string().c_str(),1)==0);
#endif
    }
    ~CacheEnvironment() {
#ifdef _WIN32
        _putenv_s(name,present?old.c_str():"");
#else
        if(present)setenv(name,old.c_str(),1);else unsetenv(name);
#endif
    }
};
json PiManifest(const std::filesystem::path& output) {
    const auto bundled=ReadJson(ProjectCreation::RuntimeDirectory()/"catalog.json");
    auto cached=json::array();for(const auto& item:bundled)if(!item.contains("uicreatorTemplate"))cached.push_back(item);
    CHECK(!cached.empty());cached.front()["description"]="Cached official template remains available";
    CacheEnvironment environment(output/"catalog-home");const auto cache=ProjectCreation::DefaultCache();std::filesystem::create_directories(cache);
    {std::ofstream file(cache/"catalog.json");file<<cached.dump();CHECK(file);}
    const auto restored=ProjectCreation::BundledCatalog();
    CHECK(std::any_of(restored.begin(),restored.end(),[](const json& item){return item.value("description",std::string{})=="Cached official template remains available";}));
    const auto matches=std::count_if(restored.begin(),restored.end(),[](const json& item){return item.value("uicreatorTemplate",std::string{})=="raspberry-pi-linux";});
    CHECK(matches==1);
    const auto found=std::find_if(restored.begin(),restored.end(),[](const json& item){return item.value("uicreatorTemplate",std::string{})=="raspberry-pi-linux";});
    CHECK(found!=restored.end());CHECK(found->at("hardware").at("chipVendor")=="Raspberry Pi");CHECK(found->at("lvglVersion")=="9.6.0");return *found;
}
void CheckPiLinux(const std::filesystem::path& output) {
    const auto manifest=PiManifest(output);CHECK(std::filesystem::is_regular_file(ProjectCreation::RuntimeDirectory()/"linux-toolkit.zip"));
    const auto project=PiProject();
    for(const char* format:{"json","c","protobuf"}) {
        const auto destination=output/(std::string("pi-linux-")+format);
        json request={{"operation","create"},{"manifest",manifest},{"destination",destination.u8string()},
            {"project",project},{"format",format},{"lvglVersion","template"},{"cache",(output/"pi-cache").u8string()}};
        const auto result=ProjectCreation::Run(request);if(!result.success)throw std::runtime_error(std::string("Pi Linux ")+format+": "+result.message);
        CHECK(std::filesystem::is_regular_file(destination/"CMakeLists.txt"));
        CHECK(std::filesystem::is_regular_file(destination/"platform/Runtime/linux/Viewer.cpp"));
        CHECK(std::filesystem::is_regular_file(destination/"platform/3rdParty/SDL3/CMakeLists.txt"));
        CHECK(ReadJson(destination/"ui/project.json")==project);
        const auto bytes=ReadBinary(destination/"ui/project.pb");CHECK(!bytes.empty());
        const auto decoded=Serialization::Protobuf::Decode(bytes);CHECK(decoded.data.at("runtimeTarget")=="9.6");CHECK(decoded.data.at("obj").size()==4);
        const auto features=ReadJson(destination/"embedded/features.json");
        CHECK(features.at("lvglSourceFileCount").get<unsigned>()<features.at("fullLvglSourceFileCount").get<unsigned>());
        const auto sections=features.at("widgetSections");CHECK(std::find(sections.begin(),sections.end(),"advancedKeyboard")!=sections.end());
        CHECK(std::find(sections.begin(),sections.end(),"textarea")!=sections.end());
        CHECK(std::filesystem::is_regular_file(destination/"embedded/vendor/LVSerial/Serialization/InteractionActions.cpp"));
        CHECK(!std::filesystem::exists(destination/"embedded/vendor/3rdParty/LVGL/lvgl/src/widgets/chart/lv_chart.c"));
        const auto config=ReadText(destination/"embedded/config/lv_conf.h");CHECK(config.find("#define LV_USE_CHART 0")!=std::string::npos);
        CHECK(config.find("#define LVSERIAL_USE_ADVANCED_KEYBOARD 1")!=std::string::npos);
        CHECK(std::filesystem::is_regular_file(destination/"ui/ui.c")==bool(std::string(format)=="c"));
        const auto instructions=ReadText(destination/"UICREATOR_GETTING_STARTED.md");
        CHECK(instructions.find("cmake -S . -B build")!=std::string::npos);CHECK(instructions.find("lvgl-pb-viewer")!=std::string::npos);
        lv_init();auto* display=lv_display_create(640,400);lvgl_runtime_t* runtime=nullptr;char error[512]{};
        if(!lvgl_runtime_load_memory(bytes.data(),bytes.size(),nullptr,&runtime,error,sizeof error))throw std::runtime_error(error);
        CHECK(lvgl_runtime_object(runtime,0,0));CHECK(lvgl_runtime_object(runtime,0,1));
        CHECK(std::string(lv_textarea_get_text(lvgl_runtime_object(runtime,0,0)))=="Linux keyboard");
        lv_obj_send_event(lvgl_runtime_object(runtime,0,3),LV_EVENT_CLICKED,nullptr);
        lvgl_runtime_value_t value{};CHECK(lvgl_runtime_get_variable(runtime,"count",&value));CHECK(value.integer_value==1);
        CHECK(std::string(lv_label_get_text(lvgl_runtime_object(runtime,0,2)))=="1");
        lvgl_runtime_destroy(runtime);lv_display_delete(display);lv_deinit();
        CHECK(!ProjectCreation::Run(request).success);
    }
    json invalid={{"operation","create"},{"manifest",manifest},{"destination",(output/"unsupported-pi").u8string()},
        {"project",project},{"format","json"},{"lvglVersion","9.5.0"},{"cache",(output/"pi-cache").u8string()}};
    CHECK(!ProjectCreation::Run(invalid).success);CHECK(!std::filesystem::exists(output/"unsupported-pi"));
}
void CheckPiPico(const std::filesystem::path& output) {
    const auto catalog=ProjectCreation::BundledCatalog();
    CHECK(std::filesystem::is_regular_file(ProjectCreation::RuntimeDirectory()/"pico-toolkit.zip"));
    const auto project=PiProject();
    for(const char* identity:{"raspberry-pi-pico","raspberry-pi-pico2"}) {
        const auto found=std::find_if(catalog.begin(),catalog.end(),[&](const json& entry){return entry.value("uicreatorTemplate",std::string{})==identity;});
        CHECK(found!=catalog.end());
        const std::string board=std::string(identity)=="raspberry-pi-pico"?"pico":"pico2";
        for(const char* format:{"c","protobuf"}) {
            const auto destination=output/(board+"-"+format);
            const json request={{"operation","create"},{"manifest",*found},{"destination",destination.u8string()},
                {"project",project},{"format",format},{"lvglVersion","template"},{"cache",(output/"pico-cache").u8string()}};
            const auto result=ProjectCreation::Run(request);
            if(!result.success)throw std::runtime_error(board+" "+format+": "+result.message);
            CHECK(ReadJson(destination/"ui/project.json")==project);
            CHECK(!ReadBinary(destination/"ui/project.pb").empty());
            CHECK(std::filesystem::is_regular_file(destination/"ui/ui.c")==bool(std::string(format)=="c"));
            CHECK(ReadText(destination/"CMakeLists.txt").find("pico_add_extra_outputs")!=std::string::npos);
            const auto metadata=ReadJson(destination/"lvgl-pb-pico-sdk.json");
            CHECK(metadata.at("board")==board);
            CHECK(metadata.at("files").contains("ui/project.pb"));
            CHECK(metadata.at("files").contains("uicreator-project.json"));
            CHECK(result.data.at("picoBuild").at("format")==format);
            CHECK(!ProjectCreation::Run(request).success);
        }
    }
}
}
int main(int argc,char** argv){try {
    CHECK(argc==4);auto root=std::filesystem::u8path(argv[1]),output=std::filesystem::u8path(argv[2]);std::filesystem::create_directories(output);
    json manifest={{"name","Offline build fixture"},{"urlToClone","https://github.com/lvgl/test-template"},{"lvglVersion","9.6.0"},{"branches",{"main"}},
        {"settings",json::array({json{{"type","dropdown"},{"label","Performance monitor"},{"options",json::array({json{{"name","On"},{"value","1"},{"default",true}}})},
        {"actions",json::array({json{{"filePath","lv_conf.h"},{"toReplace","#define LV_USE_PERF_MONITOR .*"},{"newContent","#define LV_USE_PERF_MONITOR {value}"}}})}}})}};
    json project={{"formatVersion",2},{"lvglVersion","9.6.0"},{"canvas",{{"width",320},{"height",240}}},{"obj",json::array({json{{"base",json::object()},{"arc",{{"value",37}}},{"oid",0}}})},{"variables",json::array()}};
    json request={{"operation","create"},{"manifest",manifest},{"sourceArchive",argv[3]},{"destination",(output/"c-project").u8string()},
        {"project",project},{"format","c"},{"lvglVersion","template"},{"cache",(output/"cache").u8string()}};
    auto c=ProjectCreation::Run(request);if(!c.success)throw std::runtime_error(c.message);CHECK(std::filesystem::is_regular_file(output/"c-project/ui/ui.c"));
    CHECK(std::filesystem::is_regular_file(output/"c-project/ui/project.json"));
    CHECK(!ProjectCreation::Run(request).success); // Repeating creation cannot overwrite a project.
    request["destination"]=(output/"protobuf-project").u8string();request["format"]="protobuf";auto pb=ProjectCreation::Run(request);if(!pb.success)throw std::runtime_error(pb.message);
    std::ifstream in(output/"protobuf-project/ui/project.pb",std::ios::binary);Serialization::Binary bytes((std::istreambuf_iterator<char>(in)),{});CHECK(!bytes.empty());
    auto decoded=Serialization::Protobuf::Decode(bytes);CHECK(decoded.data["runtimeTarget"]=="9.6");CHECK(decoded.data["obj"].size()==1);
    lv_init();auto* display=lv_display_create(320,240);lvgl_runtime_t* runtime=nullptr;char error[256];
    CHECK(lvgl_runtime_load_memory(bytes.data(),bytes.size(),nullptr,&runtime,error,sizeof error));CHECK(lv_arc_get_value(lvgl_runtime_object(runtime,0,0))==37);
    lvgl_runtime_destroy(runtime);lv_display_delete(display);lv_deinit();
    request["destination"]=(output/"json-project").u8string();request["format"]="json";auto data=ProjectCreation::Run(request);CHECK(data.success);CHECK(std::filesystem::is_regular_file(output/"json-project/ui/project.json"));
    CheckPiLinux(output);
    CheckPiPico(output);
    std::cout<<"Bundled creator produced JSON, C and Protobuf projects; Raspberry Pi Linux defaults generate a minimal runnable keyboard/action project and survive cached catalogs\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
