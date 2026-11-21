#include "lvgl_pb_display.h"
#include <Serialization/Protobuf.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace {
void Check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void Save(const std::filesystem::path& path,const Serialization::Binary& bytes) {
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.close();
    Check(bool(file),"Cannot write active-screen fixture");
}
void Active(lvgl_pb_window_t* window,size_t expected) {
    size_t current=SIZE_MAX;
    Check(lvgl_pb_window_active_screen(window,&current) && current==expected,"Displayed screen does not match the saved/explicit selection");
}
}
int main(int argc,char** argv) {
    try {
        Check(argc==2,"Pass an output fixture directory");
        const auto directory=std::filesystem::u8path(argv[1]);std::filesystem::create_directories(directory);
        using json=nlohmann::json;
        auto scene=[](const char* name,int width,int height,uint32_t color) {
            return json{{"name",name},{"obj",json::array()},
                {"canvas",{{"width",width},{"height",height},{"base",{{"style",{{"body",{{"main_color",color},{"opa",255}}}}}}}}}};
        };
        json document={{"formatVersion",2},{"lvglVersion","9.6.0"},{"runtimeTarget","9.6"},{"activeScreen",1},
            {"screens",{scene("First",96,64,0xff0000),scene("Second",144,88,0x00ff00)}}};
        const auto second=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",document);
        Save(directory/"active_second.pb",second);
        document["activeScreen"]=0;
        const auto first=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",document);
        Save(directory/"active_first.pb",first);
        char error[512]{};
        std::unique_ptr<lvgl_pb_window_t,decltype(&lvgl_pb_window_destroy)> window(
            lvgl_pb_window_create(320,240,"Active-screen test",error,sizeof error),lvgl_pb_window_destroy);
        Check(bool(window),error);size_t current=SIZE_MAX;
        Check(!lvgl_pb_window_active_screen(nullptr,&current) && !lvgl_pb_window_active_screen(window.get(),&current),"Unloaded window reported an active project screen");
        Check(lvgl_pb_window_load_memory(window.get(),second.data(),second.size()),lvgl_pb_window_error(window.get()));
        Check(lvgl_pb_window_screen_count(window.get())==2,"Multi-screen fixture was not loaded");Active(window.get(),1);
        Check(!lvgl_pb_window_active_screen(window.get(),nullptr),"Null active-screen output was accepted");
        Check(lvgl_pb_window_save_bmp(window.get(),(directory/"api_authored.bmp").u8string().c_str()),"Cannot capture authored screen");
        Check(lvgl_pb_window_show_screen(window.get(),0),"Explicit screen selection failed");Active(window.get(),0);
        Check(lvgl_pb_window_save_bmp(window.get(),(directory/"api_override.bmp").u8string().c_str()),"Cannot capture explicit screen");
        Check(lvgl_pb_window_load_file(window.get(),(directory/"active_second.pb").u8string().c_str()),lvgl_pb_window_error(window.get()));
        Active(window.get(),1);
        Check(lvgl_pb_window_load_memory(window.get(),first.data(),first.size()),lvgl_pb_window_error(window.get()));Active(window.get(),0);
        const uint8_t invalid[]={0xff,0xff};
        Check(!lvgl_pb_window_load_memory(window.get(),invalid,sizeof invalid),"Malformed reload was accepted");Active(window.get(),0);
        std::puts("Windows active screen, explicit selection and successful/failed reload selection passed");return 0;
    } catch(const std::exception& error) { std::fprintf(stderr,"%s\n",error.what());return 1; }
}
