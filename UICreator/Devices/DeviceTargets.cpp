#include "DeviceTargets.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <stdexcept>
namespace DeviceTargets {
json Serialize(const DeviceTools::Options& o){return {{"profile",unsigned(o.profile)},{"project",o.project.u8string()},{"build",o.build.u8string()},{"tool",o.executable.u8string()},{"python",o.python.u8string()},{"idf",o.idfRoot.u8string()},{"idfTools",o.idfTools.u8string()},{"config",o.config.u8string()},{"library",o.library.u8string()},{"toolPath",o.toolPath},{"fqbn",o.fqbn},{"port",o.port},{"baud",o.baud},{"jobs",o.jobs},{"checkIdfConstraints",o.checkIdfConstraints},{"linuxHostWsl",o.linuxHostWsl},{"runProgram",o.runProgram.u8string()},{"runArguments",o.runArguments},{"picoSdk",o.picoSdk.u8string()},{"picoBoard",o.picoBoard}};}
DeviceTools::Options Restore(const json& j){
    DeviceTools::Options o;if(!j.is_object())return o;
    const auto profile=j.value("profile",0u);if(profile>2)throw std::invalid_argument("Unknown device target toolchain");o.profile=DeviceTools::Profile(profile);
    auto p=[&](const char* name){return std::filesystem::u8path(j.value(name,std::string{}));};o.project=p("project");o.build=p("build");o.executable=p("tool");o.python=p("python");o.idfRoot=p("idf");o.idfTools=p("idfTools");o.config=p("config");o.library=p("library");o.runProgram=p("runProgram");o.toolPath=j.value("toolPath",std::string{});o.fqbn=j.value("fqbn",o.fqbn);o.port=j.value("port",std::string{});o.baud=j.value("baud",115200u);o.jobs=j.value("jobs",4u);o.checkIdfConstraints=j.value("checkIdfConstraints",true);o.linuxHostWsl=j.value("linuxHostWsl",false);o.picoSdk=p("picoSdk");o.picoBoard=j.value("picoBoard",std::string{});if(!o.picoBoard.empty()&&o.picoBoard!="pico"&&o.picoBoard!="pico2")throw std::invalid_argument("Unsupported saved Pico board");if(j.contains("runArguments"))o.runArguments=j.at("runArguments").get<std::vector<std::string>>();return o;
}
DeviceTools::Options Infer(const std::filesystem::path& project,const json& supplied,const DeviceTools::Options& defaults){
    auto o=defaults;o.project=std::filesystem::absolute(project);o.build.clear();o.runProgram.clear();o.runArguments.clear();o.linuxHostWsl=false;o.picoBoard.clear();
    auto metadata=supplied;try{if(std::ifstream file(o.project/"uicreator-project.json");file){const auto parsed=json::parse(file);metadata=parsed.value("template",parsed);}}catch(...){}
    const auto builtin=metadata.is_object()?metadata.value("uicreatorTemplate",std::string{}):std::string{};
    if(builtin=="raspberry-pi-pico")o.picoBoard="pico";
    else if(builtin=="raspberry-pi-pico2")o.picoBoard="pico2";
    else if(std::ifstream file(o.project/"lvgl-pb-pico-sdk.json");file) {
        const auto marker=json::parse(file);o.picoBoard=marker.at("board").get<std::string>();
        if(o.picoBoard!="pico"&&o.picoBoard!="pico2")throw std::invalid_argument("Unsupported generated Pico board");
    }
    auto lower=[](std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;};const auto description=lower(metadata.dump());
    if(!o.picoBoard.empty())o.profile=DeviceTools::Profile::CMake;
    else if(std::filesystem::is_regular_file(o.project/"sdkconfig.defaults")||std::filesystem::is_regular_file(o.project/"main/idf_component.yml")||description.find("esp-idf")!=std::string::npos)o.profile=DeviceTools::Profile::EspIdf;
    else if(std::filesystem::is_regular_file(o.project/"library.properties")||description.find("arduino")!=std::string::npos){o.profile=DeviceTools::Profile::Arduino;if(metadata.contains("fqbn"))o.fqbn=metadata.at("fqbn").get<std::string>();}
    else o.profile=DeviceTools::Profile::CMake;
    // A generated CMake project must not inherit an Arduino CLI executable
    // merely because Arduino was the last profile opened in the build window.
    if(o.profile!=defaults.profile)o.executable.clear();
    if(o.profile==DeviceTools::Profile::CMake&&o.picoBoard.empty()&&std::filesystem::is_regular_file(o.project/"platform/Runtime/linux/Viewer.cpp")){
        o.runProgram=o.project/"build/platform/Runtime/linux/lvgl-pb-viewer";o.runArguments={(o.project/"ui/project.pb").u8string()};
#ifdef _WIN32
        o.linuxHostWsl=true;
#endif
    }
    return DeviceTools::Detect(o);
}
std::string Save(json& doc,const std::string& name,const DeviceTools::Options& options,const std::string& replaceId){
    if(name.empty()||name.size()>100||name.find_first_of("\r\n")!=std::string::npos)throw std::invalid_argument("Enter a target name (1-100 characters)");
    auto& targets=doc["deviceTargets"];if(!targets.is_array())targets=json::array();
    std::string id=replaceId;if(id.empty()){unsigned n=1;for(;;){id="target-"+std::to_string(n++);if(std::none_of(targets.begin(),targets.end(),[&](const json& value){return value.value("id",std::string{})==id;}))break;}}
    auto value=json{{"id",id},{"name",name},{"options",Serialize(options)}};
    for(auto& target:targets)if(target.value("id",std::string{})==id){target=std::move(value);return id;}
    targets.push_back(std::move(value));return id;
}
bool Remove(json& doc,const std::string& id){if(!doc.contains("deviceTargets")||!doc["deviceTargets"].is_array())return false;auto& targets=doc["deviceTargets"];const auto before=targets.size();targets.erase(std::remove_if(targets.begin(),targets.end(),[&](const json& value){return value.value("id",std::string{})==id;}),targets.end());return before!=targets.size();}
void Progress::Feed(const std::string& output){
    pending+=output;if(pending.size()>16384)pending.erase(0,pending.size()-16384);
    size_t end;while((end=pending.find_first_of("\r\n"))!=std::string::npos){auto line=pending.substr(0,end);pending.erase(0,end+1);std::string lowered=line;for(auto& c:lowered)c=char(std::tolower(static_cast<unsigned char>(c)));
        if(lowered.rfind("> ",0)==0){percent.reset();phase=lowered.find("--version")!=std::string::npos||lowered.find(" version")!=std::string::npos?"Checking tool":lowered.find("--build")!=std::string::npos||lowered.find("\"compile\"")!=std::string::npos||lowered.find("\"build\"")!=std::string::npos?"Compiling":lowered.find("\"flash\"")!=std::string::npos||lowered.find("\"upload\"")!=std::string::npos?"Flashing":"Configuring";}
        if(lowered.find("linking")!=std::string::npos)phase="Linking";
        else if(lowered.find("building c")!=std::string::npos||lowered.find("compiling")!=std::string::npos)phase="Compiling";
        else if(lowered.find("configuring")!=std::string::npos||lowered.find("generating")!=std::string::npos)phase="Configuring";
        std::smatch match;static const std::regex fractions(R"(\[\s*(\d+)\s*/\s*(\d+)\s*\])"),percents(R"(\[\s*(\d+)\s*%\s*\])");
        if(std::regex_search(line,match,fractions)){try{const auto done=std::stoull(match[1]),total=std::stoull(match[2]);if(total&&done<=total)percent=unsigned(100.0*double(done)/double(total));}catch(...){}}
        else if(std::regex_search(line,match,percents)){try{const auto value=std::stoul(match[1]);if(value<=100)percent=unsigned(value);}catch(...){}}
    }
}
}
