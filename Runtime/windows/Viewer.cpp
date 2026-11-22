#include "lvgl_pb_display.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
namespace {
int Run(const std::vector<std::string>& args) {
    if(args.size()<2 || args[1]=="--help") {
        std::cout<<"LVGL project display\nUsage: UICreatorViewer project.pb|project.xml [--watch] [--frames N] [--screen N] [--screenshot output.bmp]\n";return args.size()<2?2:0;
    }
    bool watch=false,screenOverride=false;int frames=-1;size_t screen=0;std::string screenshot,stopFile;
    for(size_t i=2;i<args.size();++i) {
        if(args[i]=="--watch")watch=true;
        else if(args[i]=="--frames" && i+1<args.size()){frames=std::stoi(args[++i]);if(frames<0)throw std::invalid_argument("frames must be nonnegative");}
        else if(args[i]=="--screen" && i+1<args.size()){screen=std::stoul(args[++i]);screenOverride=true;}
        else if(args[i]=="--screenshot" && i+1<args.size())screenshot=args[++i];
        else if(args[i]=="--stop-file" && i+1<args.size())stopFile=args[++i];
        else throw std::invalid_argument("Unknown viewer argument: "+args[i]);
    }
    char error[512]{};
    std::unique_ptr<lvgl_pb_window_t,decltype(&lvgl_pb_window_destroy)> window(lvgl_pb_window_create(320,240,"LVGL Protobuf Debug",error,sizeof error),lvgl_pb_window_destroy);
    if(!window){std::cerr<<error<<'\n';return 1;}
    auto load=[&] {
        if(!lvgl_pb_window_load_file(window.get(),args[1].c_str())) {std::cerr<<lvgl_pb_window_error(window.get())<<'\n';return false;}
        if(screenOverride && !lvgl_pb_window_show_screen(window.get(),screen)){std::cerr<<"Screen index is unavailable\n";return false;}
        size_t active=0;
        if(!lvgl_pb_window_active_screen(window.get(),&active)){std::cerr<<"No project screen is active\n";return false;}
        const auto title="Protobuf Debug | "+std::filesystem::u8path(args[1]).filename().u8string()+" | screen "+std::to_string(active+1)+"/"+std::to_string(lvgl_pb_window_screen_count(window.get()));
        lvgl_pb_window_set_title(window.get(),title.c_str());return true;
    };
    if(!load())return 1;
    std::error_code fileError;auto modified=std::filesystem::last_write_time(std::filesystem::u8path(args[1]),fileError);
    auto nextCheck=std::chrono::steady_clock::now();
    for(int frame=0;(frames<0 || frame<frames) && lvgl_pb_window_poll(window.get());++frame) {
        if(!stopFile.empty() && std::filesystem::exists(std::filesystem::u8path(stopFile),fileError))break;
        if(watch && std::chrono::steady_clock::now()>=nextCheck) {
            nextCheck=std::chrono::steady_clock::now()+std::chrono::milliseconds(250);
            const auto changed=std::filesystem::last_write_time(std::filesystem::u8path(args[1]),fileError);
            if(!fileError && changed!=modified){modified=changed;load();}
        }
    }
    if(!screenshot.empty() && !lvgl_pb_window_save_bmp(window.get(),screenshot.c_str())){std::cerr<<lvgl_pb_window_error(window.get())<<'\n';return 1;}
    return 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    try {
        std::vector<std::string> args;
        for(int i=0;i<argc;++i) {
            int count=WideCharToMultiByte(CP_UTF8,0,argv[i],-1,nullptr,0,nullptr,nullptr);
            std::string value(count,'\0');WideCharToMultiByte(CP_UTF8,0,argv[i],-1,value.data(),count,nullptr,nullptr);value.pop_back();args.push_back(std::move(value));
        }
        return Run(args);
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
#else
int main(int argc,char** argv) {try{return Run({argv,argv+argc});}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}}
#endif
