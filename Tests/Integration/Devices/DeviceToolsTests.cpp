#include "../../../UICreator/Devices/DeviceTools.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace DeviceTools;
void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void CheckProcessTrees(Command child, const std::filesystem::path& temporary) {
    const auto siblingFile = temporary / "unrelated.txt";
    std::filesystem::remove(siblingFile);
    Command sibling = child; sibling.arguments = {"--child", "heartbeat", siblingFile.u8string()};
    std::atomic_bool siblingCancel{false};
    std::thread siblingWorker([&] { Run({sibling}, siblingCancel, {}); });
    struct StopSibling {
        std::atomic_bool& cancel; std::thread& worker;
        ~StopSibling() { cancel = true; if (worker.joinable()) worker.join(); }
    } stopSibling{siblingCancel, siblingWorker};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!std::filesystem::exists(siblingFile) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Check(std::filesystem::exists(siblingFile), "Unrelated concurrent helper did not start");
    for (const auto depth : {1, 3}) {
        const auto heartbeat = temporary / ("grandchild-" + std::to_string(depth) + ".txt");
        std::filesystem::remove(heartbeat);
        child.arguments = {"--child", depth == 1 ? "tree" : "tree-thread", heartbeat.u8string(), std::to_string(depth)};
        std::atomic_bool cancel{false};
        const auto started = std::chrono::steady_clock::now();
        const auto result = Run({child}, cancel, [&](const std::string& text) {
            if (text.find("tick") != std::string::npos) cancel = true;
        });
        Check(result.error.empty(), result.error.c_str());
        Check(result.cancelled && std::filesystem::exists(heartbeat), "Cancellation tree helper failed");
        Check(std::chrono::steady_clock::now() - started < std::chrono::seconds(5), "Tree cancellation remains responsive");
        const auto heartbeatSize = std::filesystem::file_size(heartbeat);
        const auto siblingSize = std::filesystem::file_size(siblingFile);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        Check(std::filesystem::file_size(heartbeat) == heartbeatSize, "Cancellation orphaned a grandchild process");
        Check(std::filesystem::file_size(siblingFile) > siblingSize, "Cancellation stopped an unrelated process");
        std::filesystem::remove(heartbeat);
    }
    siblingCancel = true; siblingWorker.join();
    std::filesystem::remove(siblingFile);
}
int main(int argc,char** argv){
    if(argc>1&&std::string(argv[1])=="--child") {
        if (argc > 3 && (std::string(argv[2]) == "tree" || std::string(argv[2]) == "tree-thread")) {
            const bool threaded = std::string(argv[2]) == "tree-thread";
            Command grandchild; grandchild.executable = std::filesystem::absolute(argv[0]);
            const auto depth = argc > 4 ? std::stoi(argv[4]) : 1;
            grandchild.arguments = depth > 1 ?
                std::vector<std::string>{"--child", threaded ? "tree-thread" : "tree", argv[3], std::to_string(depth - 1)} :
                std::vector<std::string>{"--child", "heartbeat", argv[3]};
            const auto execute = [&] {
                std::atomic_bool cancel{false};
                return Run({grandchild}, cancel, [](const std::string& text) { std::cout << text << std::flush; }).exitCode;
            };
            if (!threaded) return execute();
            int result = -1; std::thread worker([&] { result = execute(); }); worker.join(); return result;
        }
        if(argc>3&&std::string(argv[2])=="heartbeat"){std::ofstream output(std::filesystem::u8path(argv[3]),std::ios::binary|std::ios::app);for(int i=0;i<800;++i){output<<'.'<<std::flush;std::cout<<"tick\n"<<std::flush;std::this_thread::sleep_for(std::chrono::milliseconds(20));}return 0;}
        if(argc>2&&std::string(argv[2])=="wait"){for(int i=0;i<800;++i){std::cout<<"tick\n"<<std::flush;std::this_thread::sleep_for(std::chrono::milliseconds(20));}return 0;}
        for(int i=2;i<argc;++i)std::cout<<'['<<argv[i]<<"]\n";
        std::cout<<"environment="<<(std::getenv("DEVICE_TOOLS_TEST")?std::getenv("DEVICE_TOOLS_TEST"):"")<<"\n"<<std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));std::cerr<<"last chunk\n"<<std::flush;return 17;
    }
    if(argc==5&&(std::string(argv[1])=="--build-arduino"||std::string(argv[1])=="--build-idf")){Options options;options.profile=std::string(argv[1])=="--build-idf"?Profile::EspIdf:Profile::Arduino;options=Detect(options,std::filesystem::u8path(argv[2]));options.project=std::filesystem::u8path(argv[3]);options.build=std::filesystem::u8path(argv[4]);std::atomic_bool cancel{false};auto result=Run(BuildPlan(options),cancel,[](const std::string& text){std::cout<<text<<std::flush;});if(!result.error.empty())std::cerr<<result.error;return result.exitCode;}
    try{
        const auto self=std::filesystem::absolute(argv[0]);const auto workspace=argc>1?std::filesystem::absolute(argv[1]):std::filesystem::current_path();
        const auto temporary=self.parent_path()/"project $() & with spaces";std::filesystem::create_directories(temporary);
        Options options;options.project=temporary;options.executable=self;options.port="COM99";
        auto build=BuildPlan(options);Check(build.size()==1,"Arduino build plan");Check(build[0].arguments[0]=="compile","Compile action");Check(build[0].arguments[2]=="arduino:esp32:nano_nora","Nano ESP32 FQBN");
        auto flash=BuildPlan(options,true);Check(flash[0].arguments[0]=="upload","Flash action");Check(flash[0].arguments[4]=="COM99","Flash configured port");
        options.port.clear();bool rejected=false;try{BuildPlan(options,true);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"No flash without explicit port");
        options.profile=Profile::CMake;build=BuildPlan(options);Check(build.size()==2&&build[1].arguments[0]=="--build","CMake configure and build");
        rejected=false;try{BuildPlan(options,true);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"No generic CMake flash");
        Command child;child.executable=self;child.directory=temporary;child.arguments={"--child","spaces and \"quotes\"","trailing\\","$(echo injected) & ; %PATH%","Ελληνικά"};child.environment["DEVICE_TOOLS_TEST"]="value with spaces & quotes\"";
        std::atomic_bool cancel{false};std::string log;auto result=Run({child},cancel,[&](const std::string& text){log+=text;});
        Check(result.error.empty(),result.error.c_str());Check(result.exitCode==17,"Child exit code retained");Check(log.find("[spaces and \"quotes\"]")!=std::string::npos,"Quoted argument retained");Check(log.find("[trailing\\]")!=std::string::npos,"Trailing slash retained");Check(log.find("[$(echo injected) & ; %PATH%]")!=std::string::npos,"Shell syntax remains literal");Check(log.find("environment=value with spaces & quotes\"")!=std::string::npos,"Native environment forwarded");Check(log.find("last chunk")!=std::string::npos,"stdout and stderr drained");
        child.arguments={"--child","wait"};log.clear();const auto start=std::chrono::steady_clock::now();result=Run({child},cancel,[&](const std::string& text){log+=text;if(text.find("tick")!=std::string::npos)cancel=true;});Check(result.cancelled,"Process cancellation returned");Check(log.find("tick")!=std::string::npos,"Streaming output before child exit");Check(std::chrono::steady_clock::now()-start<std::chrono::seconds(5),"Cancellation remains responsive");
        CheckProcessTrees(child, temporary);
        for(auto profile:{Profile::Arduino,Profile::CMake,Profile::EspIdf}){Options probe;probe.profile=profile;probe=Detect(probe,workspace);if((profile==Profile::EspIdf&&(probe.python.empty()||probe.idfRoot.empty()))||(profile!=Profile::EspIdf&&probe.executable.empty()))continue;probe.project=workspace;cancel=false;log.clear();std::cout<<"Detected IDF="<<probe.idfRoot<<" Python="<<probe.python<<'\n';result=Run(ProbePlan(probe),cancel,[&](const std::string& text){log+=text;});std::cout<<log;Check(result.error.empty(),result.error.c_str());Check(result.exitCode==0,"Detected tool --version succeeds");}
        // This only enumerates OS port names; it never opens or flashes hardware.
        std::cout<<"Discovered "<<SerialPorts().size()<<" serial ports\n";
        std::filesystem::remove(temporary);std::cout<<"Device process checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
