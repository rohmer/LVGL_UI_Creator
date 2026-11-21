#include <RuntimeCompat.h>
#include "lvgl_runtime.h"
#include <Serialization/Protobuf.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error(std::string("Line ")+std::to_string(__LINE__)+": " #x);}while(0)
using namespace Serialization;
#if LVGL_VERSION_MAJOR == 8
static void Flush(lv_disp_drv_t* d,const lv_area_t*,lv_color_t*){lv_disp_flush_ready(d);}
#else
static void Flush(lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);}
#endif
int main(int argc,char** argv) {
 std::string current;
 try {
    CHECK(argc==2);lv_init();static uint32_t pixels[320*32];
#if LVGL_VERSION_MAJOR == 8
    static lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,pixels,nullptr,320*32);
    static lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=320;driver.ver_res=240;driver.flush_cb=Flush;driver.draw_buf=&draw;
    auto* display=lv_disp_drv_register(&driver);
#else
    auto* display=lv_display_create(320,240);lv_display_set_flush_cb(display,Flush);
    lv_display_set_buffers(display,pixels,nullptr,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
    auto* previous=lv_screen_active();size_t images=0,fonts=0;
    for(auto& entry:std::filesystem::directory_iterator(argv[1])) {
        if(entry.path().extension()!=".pb")continue;current=entry.path().filename().string();
        std::ifstream file(entry.path(),std::ios::binary);Binary input((std::istreambuf_iterator<char>(file)),{});
        auto doc=Protobuf::Decode(input).data;doc["runtimeTarget"]=lvgl_runtime_backend();
        auto bytes=Protobuf::Encode(EntityKind::Project,"project",doc);
        lvgl_runtime_t* runtime=nullptr;char error[512];
        if(!lvgl_runtime_load_memory(bytes.data(),bytes.size(),nullptr,&runtime,error,sizeof error))throw std::runtime_error(error);
        auto* object=lvgl_runtime_object(runtime,0,0);CHECK(object);
        if(doc.contains("images")) {
            auto* image=lvgl_runtime_image(runtime,"image_1");CHECK(image);CHECK(lv_image_get_src(object)==image);++images;
        } else {
            auto* font=lvgl_runtime_font(runtime,"font_1");CHECK(font);CHECK(lv_obj_get_style_text_font(object,LV_PART_MAIN)==font);
            lv_font_glyph_dsc_t glyph{};CHECK(lv_font_get_glyph_dsc(font,&glyph,'A','V'));CHECK(glyph.adv_w);
#if LVGL_VERSION_MAJOR == 8
            CHECK(lv_font_get_glyph_bitmap(font,'A'));
#else
            auto* bitmap=lv_draw_buf_create(glyph.box_w,glyph.box_h,LV_COLOR_FORMAT_A8,0);CHECK(bitmap);
#if LVGL_VERSION_MINOR == 1
            CHECK(lv_font_get_glyph_bitmap(&glyph,'A',bitmap));
#else
            CHECK(lv_font_get_glyph_bitmap(&glyph,bitmap));
#endif
#if LVGL_VERSION_MINOR >= 3
            lv_font_glyph_release_draw_data(&glyph);
#endif
            lv_draw_buf_destroy(bitmap);
#endif
            ++fonts;
        }
        lv_obj_update_layout(lvgl_runtime_screen(runtime,0));lv_refr_now(nullptr);
        lvgl_runtime_destroy(runtime);CHECK(lv_screen_active()==previous);
    }
    CHECK(images==59);CHECK(fonts==10);
#if LVGL_VERSION_MAJOR == 8
    lv_disp_remove(display);
#else
    lv_display_delete(display);lv_deinit();
#endif
    std::cout<<images<<" image combinations and "<<fonts<<" font combinations replayed\n";return 0;
 }catch(const std::exception& e){std::cerr<<current<<": "<<e.what()<<'\n';return 1;}
}
