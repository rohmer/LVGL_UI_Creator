#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { fprintf(stderr,"Code generated C failed at line %d: %s\n",__LINE__,#value); return 1; } } while (0)
static void Flush(lv_display_t* display, const lv_area_t* area, uint8_t* data) { (void)area;(void)data;lv_display_flush_ready(display); }
int main(void)
{
    unsigned pass;
    lv_init();
    lv_display_t* display=lv_display_create(640,480);
    CHECK(display);
    void* pixels=malloc(640*16*4);CHECK(pixels);
    lv_display_set_buffers(display,pixels,NULL,640*16*4,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,Flush);
    lv_obj_t* original=lv_screen_active();
    for(pass=0;pass<3;++pass) {
        CHECK(ui_init());CHECK(ui_init());
        lv_obj_t* barcode=ui_get_object(0,0);lv_obj_t* qr=ui_get_object(0,1);
        CHECK(barcode && qr);
        CHECK(lv_obj_get_class(barcode)==&lv_barcode_class && lv_obj_get_class(qr)==&lv_qrcode_class);
        CHECK(lv_barcode_is_render_valid(barcode) && lv_qrcode_is_render_valid(qr));
        CHECK(lv_barcode_get_scale(barcode)==2 && lv_barcode_get_encoding(barcode)==LV_BARCODE_ENCODING_CODE128_RAW);
        CHECK(lv_color_to_int(lv_barcode_get_dark_color(barcode))==0x123456);
        CHECK(lv_obj_get_width(qr)==160 && lv_obj_get_height(qr)==160);
        CHECK(lv_canvas_get_draw_buf(barcode) && lv_canvas_get_draw_buf(qr));
        CHECK(strcmp(lv_barcode_get_data(barcode),"87654321")==0);
        uint32_t count=0;const void* payload=lv_qrcode_get_data(qr,&count);
        CHECK(count==10 && payload && memcmp(payload,"ESP\0Device",10)==0);
        lv_refr_now(display);
        ui_deinit();CHECK(lv_screen_active()==original);ui_deinit();
    }
    lv_display_delete(display);lv_deinit();free(pixels);
    puts("Generated Code 128 and QR C11 widgets rendered and cleaned up repeatedly");return 0;
}
