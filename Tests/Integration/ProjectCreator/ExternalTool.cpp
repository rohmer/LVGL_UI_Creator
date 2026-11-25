#include "../../../UICreator/Utils/ExternalTool.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void Require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}
}

int main(int argc,char** argv) {
    if(argc>1 && std::string(argv[1])=="--wait") {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 0;
    }
    if(argc>1 && std::string(argv[1])=="--success") {
        std::cout<<"helper succeeded\n";
        return 0;
    }
    if(argc>1 && std::string(argv[1])=="--failure")return 7;
    try {
        Require(argc==2,"Expected the test output directory");
        const auto self=std::filesystem::absolute(std::filesystem::u8path(argv[0]));
        const auto directory=std::filesystem::u8path(argv[1])/"paths with spaces";
        std::filesystem::create_directories(directory);
        const auto log=directory/"helper.log";
        const auto request=directory/"request.json";
        Require(ExternalTool::Run(self,"--success",request,log)==0,"Successful helper failed");
        std::ifstream stream(log);
        std::string line;std::getline(stream,line);
        Require(line=="helper succeeded","Helper output was not redirected");
        stream.close();
        Require(ExternalTool::Run(self,"--failure",request,log)==7,"Helper exit code was lost");
        bool invalid=false;
        try {ExternalTool::Run(self,"--wait",request,log,nullptr,std::chrono::milliseconds(0));}
        catch(const std::invalid_argument&) {invalid=true;}
        Require(invalid,"A non-positive deadline was accepted");
        const auto start=std::chrono::steady_clock::now();
        bool timedOut=false;
        try {ExternalTool::Run(self,"--wait",request,log,nullptr,std::chrono::milliseconds(50));}
        catch(const std::runtime_error& error) {timedOut=std::string(error.what())=="Project tool timed out";}
        Require(timedOut,"The explicit short deadline did not terminate the helper");
        Require(std::chrono::steady_clock::now()-start<std::chrono::seconds(5),"The short deadline was not enforced promptly");
        std::atomic<bool> cancel{false};
        std::thread cancellation([&]{std::this_thread::sleep_for(std::chrono::milliseconds(100));cancel=true;});
        bool cancelled=false;
        try {ExternalTool::Run(self,"--wait",request,log,&cancel,std::chrono::seconds(5));}
        catch(const std::runtime_error& error) {cancelled=std::string(error.what()).find("cancelled")!=std::string::npos;}
        catch(...) {cancellation.join();throw;}
        cancellation.join();
        Require(cancelled,"Cancellation did not terminate the helper before its deadline");
        std::cout<<"Project helper exit status, output, deadline and cancellation passed\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
