#include "../../../UICreator/Devices/DeviceTargets.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#define CHECK(value) do{if(!(value))throw std::runtime_error(std::string("Line ")+std::to_string(__LINE__)+": " #value);}while(0)
int main(int argc,char** argv){try{
    namespace fs=std::filesystem;using json=nlohmann::json;
    CHECK(argc==2);const auto workspace=fs::absolute(fs::u8path(argv[1]));fs::create_directories(workspace);
    DeviceTools::Options original;original.profile=DeviceTools::Profile::CMake;original.project=workspace/"project with spaces";original.build=workspace/"build";original.port="COM9";original.toolPath="path with ; literal ` and $(text)";original.runProgram=workspace/"viewer";original.runArguments={"argument with spaces","literal $(text)"};original.linuxHostWsl=true;
    const auto restored=DeviceTargets::Restore(DeviceTargets::Serialize(original));CHECK(DeviceTargets::Serialize(restored)==DeviceTargets::Serialize(original));
    json document={{"canvas",{{"width",320},{"height",240}}},{"obj",json::array()}};const auto first=DeviceTargets::Save(document,"Desktop",original);auto second=DeviceTargets::Save(document,"Desktop copy",original);CHECK(first!=second&&document["deviceTargets"].size()==2);original.port="COM10";CHECK(DeviceTargets::Save(document,"Renamed",original,first)==first);CHECK(document["deviceTargets"][0]["name"]=="Renamed");CHECK(document["deviceTargets"][0]["options"]["port"]=="COM10");CHECK(DeviceTargets::Remove(document,second)&&document["deviceTargets"].size()==1);CHECK(!DeviceTargets::Remove(document,second));
    DeviceTargets::Progress progress;progress.Feed("> cmake -S project\n-- Configuring done\n");CHECK(progress.phase=="Configuring"&&!progress.percent);progress.Feed("[2/");CHECK(!progress.percent);progress.Feed("10] Building CXX object file\n");CHECK(progress.phase=="Compiling"&&progress.percent&&*progress.percent==20);progress.Feed("[90%] Linking C executable\n");CHECK(progress.phase=="Linking"&&progress.percent&&*progress.percent==90);progress.Feed("> cmake --build build\n");CHECK(!progress.percent);progress.Feed("Tool output without a progress report\n");CHECK(!progress.percent);
    CHECK(DeviceTools::WslPath(fs::path("E:/source/my project"))=="/mnt/e/source/my project");CHECK(DeviceTools::WslPath(fs::path("/var/tmp/project"))=="/var/tmp/project");bool rejected=false;try{DeviceTools::WslPath("relative/path");}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
    const auto cmake=workspace/"cmake-project";fs::create_directories(cmake/"platform/Runtime/linux");{std::ofstream file(cmake/"platform/Runtime/linux/Viewer.cpp");file<<"// bundled Linux transport\n";}auto inferred=DeviceTargets::Infer(cmake,{{"uicreatorTemplate","raspberry-pi-linux"}},original);CHECK(inferred.profile==DeviceTools::Profile::CMake);CHECK(inferred.project==fs::absolute(cmake));CHECK(inferred.runProgram==inferred.project/"build/platform/Runtime/linux/lvgl-pb-viewer");CHECK(inferred.runArguments==std::vector<std::string>{(inferred.project/"ui/project.pb").u8string()});
    auto stale=original;stale.profile=DeviceTools::Profile::Arduino;stale.executable="arduino-cli.exe";CHECK(DeviceTargets::Infer(cmake,{},stale).executable!=stale.executable);
    const auto idf=workspace/"idf-project";fs::create_directories(idf);{std::ofstream file(idf/"sdkconfig.defaults");file<<"";}CHECK(DeviceTargets::Infer(idf).profile==DeviceTools::Profile::EspIdf);
    const auto arduino=workspace/"arduino-project";fs::create_directories(arduino);{std::ofstream file(arduino/"library.properties");file<<"name=Example\n";}CHECK(DeviceTargets::Infer(arduino).profile==DeviceTools::Profile::Arduino);
    std::cout<<"Named target persistence, source-specific toolchain/run inference, structured WSL paths and honest phase/progress parsing passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
