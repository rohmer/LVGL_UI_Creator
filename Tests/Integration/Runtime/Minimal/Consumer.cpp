#include <lvgl_pb_esp32.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Protobuf.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>

static void Require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
static void Flush(lv_display_t* display,const lv_area_t*,uint8_t*) { lv_display_flush_ready(display); }
int main()
{
    try {
#if LV_USE_CHART || LV_USE_TABLE || LV_USE_BAR || LV_USE_ARC || LV_USE_CANVAS || LV_USE_BUTTON || LVSERIAL_USE_ADVANCED_KEYBOARD
#error This consumer must be linked to a label-only profile
#endif
        const auto types=Serialization::ObjectSerializer::SupportedTypes();
        Require(std::find(types.begin(),types.end(),"label")!=types.end(),"Label support is missing");
        Require(std::find(types.begin(),types.end(),"chart")==types.end(),"Unused chart entered minimal registry");
        Require(std::find(types.begin(),types.end(),"advancedKeyboard")==types.end(),"Unused Advanced Keyboard entered minimal registry");
        lv_init();
        auto* display=lv_display_create(320,240);
        Require(display,"Cannot allocate display");
        const auto bytes=320u*16u*2u;
        auto* pixels=std::malloc(bytes);Require(pixels,"Cannot allocate display buffer");
        lv_display_set_buffers(display,pixels,nullptr,bytes,LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display,Flush);
        Require(lvgl_pb_embedded_project_count>0,"No embedded fixture");
        const auto& project=lvgl_pb_embedded_projects[0];
        lvgl_runtime_t* loaded=nullptr;char error[512]{};
        const auto unsupported=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",
            {{"obj",{{{"oid",0},{"par",-1},{"base",nlohmann::json::object()},{"chart",nlohmann::json::object()}}}}});
        const auto unsupportedKeyboard=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",
            {{"obj",{{{"oid",0},{"par",-1},{"base",nlohmann::json::object()},{"advancedKeyboard",nlohmann::json::object()}}}}});
        for(int attempt=0;attempt<3;++attempt) {
            Require(lvgl_runtime_load_memory(project.data,project.size,nullptr,&loaded,error,sizeof error),error);
            int32_t width=0,height=0;
            Require(lvgl_runtime_screen_size(loaded,0,&width,&height) && width>0 && height>0,"Authored screen size missing");
            lv_display_set_resolution(display,400,300);
            int32_t unchangedWidth=0,unchangedHeight=0;
            Require(lvgl_runtime_screen_size(loaded,0,&unchangedWidth,&unchangedHeight) && width==unchangedWidth && height==unchangedHeight,"Authored dimensions changed with display resize");
            auto* root=lvgl_runtime_screen(loaded,0);Require(root,"Loaded screen missing");
            lv_obj_update_layout(root);lv_tick_inc(20);lv_timer_handler();lv_refr_now(display);
            lvgl_runtime_t* rejected=reinterpret_cast<lvgl_runtime_t*>(1);
            Require(!lvgl_runtime_load_memory(unsupported.data(),unsupported.size(),nullptr,&rejected,error,sizeof error) && !rejected,"Disabled chart project was accepted");
            Require(!lvgl_runtime_load_memory(unsupportedKeyboard.data(),unsupportedKeyboard.size(),nullptr,&rejected,error,sizeof error) && !rejected,"Disabled Advanced Keyboard project was accepted");
            Require(lv_screen_active()==root,"Rejected project changed the active screen");
            Require(lvgl_runtime_screen(loaded,0)==root,"Rejected project destroyed the live project");
            lvgl_runtime_destroy(loaded);loaded=nullptr;
            for(int bad=0;bad<16;++bad) {
                const uint8_t invalid[]={0xff,0xff,0xff};
                Require(!lvgl_runtime_load_memory(invalid,sizeof invalid,nullptr,&loaded,error,sizeof error) && !loaded,"Malformed data was accepted");
            }
        }
        lv_display_delete(display);lv_deinit();std::free(pixels);
        std::puts("Minimal label-only link/render/lifetime, feature rejection and authored dimensions passed");
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
