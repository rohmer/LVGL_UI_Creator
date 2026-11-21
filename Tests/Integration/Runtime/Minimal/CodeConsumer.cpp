#include <lvgl_pb_esp32.h>
#include <Serialization/CodeWidgets.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Protobuf.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#if !LV_USE_BARCODE || !LV_USE_QRCODE || !LV_USE_CANVAS || !LV_USE_IMAGE
#error Code-widget profiles require barcode, QR, canvas and image support
#endif
#if LV_USE_CHART || LV_USE_TABLE || LV_USE_ARC || LV_USE_BAR || LV_USE_KEYBOARD || LV_USE_TABVIEW
#error Unrelated native widgets must remain excluded from this code-only profile
#endif
#if !LV_DRAW_SW_SUPPORT_I1 || !LV_DRAW_SW_SUPPORT_RGB565
#error Indexed code bitmaps and RGB565 output must both be enabled
#endif

namespace {
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
struct Probe {
    lv_area_t area{};
    uint16_t dark=0,light=0;
    size_t darkPixels=0,lightPixels=0;
};
Probe probes[2];
bool capture=false;
size_t flushes=0;
void Flush(lv_display_t* display,const lv_area_t* area,uint8_t* pixels)
{
    ++flushes;
    if(capture) {
        const auto* buffer=lv_display_get_buf_active(display);
        const size_t stride=buffer->header.stride;
        for(auto& probe:probes) {
            const int x1=(std::max)(area->x1,probe.area.x1),x2=(std::min)(area->x2,probe.area.x2);
            const int y1=(std::max)(area->y1,probe.area.y1),y2=(std::min)(area->y2,probe.area.y2);
            for(int y=y1;y<=y2;++y) for(int x=x1;x<=x2;++x) {
                uint16_t color;
                std::memcpy(&color,pixels+size_t(y-area->y1)*stride+size_t(x-area->x1)*2,sizeof color);
                if(color==probe.dark) ++probe.darkPixels;
                if(color==probe.light) ++probe.lightPixels;
            }
        }
    }
    lv_display_flush_ready(display);
}
void Watch(lv_obj_t* object,Probe& probe,bool barcode)
{
    Require(object,"Code object lookup failed");
    Require(barcode ? lv_barcode_is_render_valid(object) : lv_qrcode_is_render_valid(object),"Native code encoder failed");
    const auto* buffer=lv_canvas_get_draw_buf(object);
    Require(buffer && buffer->data && buffer->data_size,"Code bitmap was not allocated");
    Require(buffer->header.cf==LV_COLOR_FORMAT_I1,"Code bitmap did not use indexed monochrome storage");
    lv_obj_get_coords(object,&probe.area);
    probe.dark=lv_color_to_u16(barcode ? lv_barcode_get_dark_color(object) : lv_qrcode_get_dark_color(object));
    probe.light=lv_color_to_u16(barcode ? lv_barcode_get_light_color(object) : lv_qrcode_get_light_color(object));
    probe.darkPixels=probe.lightPixels=0;
    Require(probe.dark!=probe.light,"Code colors lost contrast in RGB565");
}
}

int main()
{
    try {
        const auto supported=Serialization::ObjectSerializer::SupportedTypes();
        Require(std::find(supported.begin(),supported.end(),"barcode")!=supported.end(),"Barcode is missing from the minimal registry");
        Require(std::find(supported.begin(),supported.end(),"qrcode")!=supported.end(),"QR is missing from the minimal registry");
        Require(std::find(supported.begin(),supported.end(),"chart")==supported.end(),"Unused chart entered the minimal registry");
        Require(lvgl_pb_embedded_project_count==1,"Expected one code fixture");
        const auto& input=lvgl_pb_embedded_projects[0];
        lv_init();
        auto* display=lv_display_create(640,480);Require(display,"Cannot create display");
        lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
        const size_t bufferSize=640u*16u*2u;
        auto* pixels=std::malloc(bufferSize);Require(pixels,"Cannot allocate display buffer");
        lv_display_set_buffers(display,pixels,nullptr,static_cast<uint32_t>(bufferSize),LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display,Flush);
        auto* previous=lv_screen_active();
        const auto unsupported=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",
            {{"obj",{{{"oid",0},{"par",-1},{"base",nlohmann::json::object()},{"chart",nlohmann::json::object()}}}}});
        for(int cycle=0;cycle<3;++cycle) {
            char error[512]{};lvgl_runtime_t* runtime=nullptr;
            Require(lvgl_runtime_load_memory(input.data,input.size,nullptr,&runtime,error,sizeof error),error);
            auto* barcode=lvgl_runtime_object(runtime,0,0);auto* qr=lvgl_runtime_object(runtime,0,1);
            Require(barcode && qr,"Runtime omitted code widgets");
            Require(lv_obj_get_class(barcode)==&lv_barcode_class && lv_obj_get_class(qr)==&lv_qrcode_class,"Runtime code widget classes differ");
            const auto* barcodeData=lv_barcode_get_data(barcode);
            Require(barcodeData && std::strcmp(barcodeData,"87654321")==0,"Barcode payload changed");
            uint32_t qrLength=0;const auto* qrData=lv_qrcode_get_data(qr,&qrLength);
            const char expectedQr[]={'E','S','P','\0','D','e','v','i','c','e'};
            Require(qrData && qrLength==sizeof expectedQr && std::memcmp(qrData,expectedQr,sizeof expectedQr)==0,"Binary QR payload changed");
            auto* root=lvgl_runtime_screen(runtime,0);Require(root,"Runtime screen missing");
            lv_obj_update_layout(root);Watch(barcode,probes[0],true);Watch(qr,probes[1],false);
            const auto previousFlushes=flushes;
            capture=true;lv_obj_invalidate(root);lv_tick_inc(20);lv_timer_handler();lv_refr_now(display);capture=false;
            Require(flushes>previousFlushes,"Display did not flush rendered pixels");
            for(const auto& probe:probes)
                Require(probe.darkPixels>0 && probe.lightPixels>0,"Indexed code bitmap did not render both colors into the RGB565 display");
            lvgl_runtime_t* rejected=reinterpret_cast<lvgl_runtime_t*>(1);
            Require(!lvgl_runtime_load_memory(unsupported.data(),unsupported.size(),nullptr,&rejected,error,sizeof error) && !rejected,"Disabled chart was accepted");
            Require(lv_screen_active()==root,"Rejected input changed the active code screen");
            lvgl_runtime_destroy(runtime);
            Require(lv_screen_active()==previous,"Destroy did not restore the previous screen");
        }
        lv_display_delete(display);lv_deinit();std::free(pixels);
        std::puts("Minimal barcode/QR link, I1-to-RGB565 pixels, payloads, lifetimes and disabled-widget rejection passed");
        return 0;
    } catch(const std::exception& error) { std::fprintf(stderr,"%s\n",error.what());return 1; }
}
