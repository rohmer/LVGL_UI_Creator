#include "ui.h"
#include <src/image/lv_image_decoder_private.h>
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    const lv_image_dsc_t* image;lv_display_t* display;lv_init();display=lv_display_create(800,600);CHECK(ui_init());
    CHECK(ui_get_font("font_1")!=NULL);CHECK(lv_obj_get_style_text_font(ui_get_object(0,0),LV_PART_MAIN)==ui_get_font("font_1"));
    CHECK(lv_obj_get_style_text_font(ui_get_object(0,1),LV_PART_MAIN)==ui_get_font("font_1"));
    CHECK(ui_get_font("font_alias")==ui_get_font("font_1"));CHECK(ui_get_font(NULL)==NULL);CHECK(ui_get_font("missing")==NULL);CHECK(ui_get_font("font_unused")==NULL);
    {lv_font_glyph_dsc_t glyph;lv_draw_buf_t* bitmap;CHECK(ui_get_font("font_compressed"));
        lv_obj_add_state(ui_get_object(0,0),LV_STATE_PRESSED);CHECK(lv_obj_get_style_text_font(ui_get_object(0,0),LV_PART_MAIN)==ui_get_font("font_compressed"));
        CHECK(lv_font_get_glyph_dsc(ui_get_font("font_compressed"),&glyph,'A',0));CHECK(glyph.box_w==1&&glyph.box_h==1&&glyph.adv_w==1);
        bitmap=lv_draw_buf_create(1,1,LV_COLOR_FORMAT_A8,LV_STRIDE_AUTO);CHECK(bitmap);CHECK(lv_font_get_glyph_bitmap(&glyph,bitmap));CHECK(bitmap->data[0]==255);
        lv_font_glyph_release_draw_data(&glyph);lv_draw_buf_destroy(bitmap);lv_obj_remove_state(ui_get_object(0,0),LV_STATE_PRESSED);}
    CHECK(ui_get_image("image_1")!=NULL);CHECK(ui_get_image("missing")==NULL);CHECK(lv_image_get_src(ui_get_object(0,6))==ui_get_image("image_1"));
    {int state;lv_obj_t* button=ui_get_object(0,7);CHECK(button!=NULL);for(state=0;state<LV_IMAGEBUTTON_STATE_NUM;++state){CHECK(lv_imagebutton_get_src_left(button,state)==ui_get_image("image_1"));CHECK(lv_imagebutton_get_src_middle(button,state)==ui_get_image("image_1"));CHECK(lv_imagebutton_get_src_right(button,state)==ui_get_image("image_1"));}}
    image=lv_image_get_src(ui_get_object(0,2));CHECK(image->header.cf==LV_COLOR_FORMAT_RGB565A8);CHECK(image->header.flags&LV_IMAGE_FLAGS_COMPRESSED);CHECK(image->header.stride==8);
    image=lv_image_get_src(ui_get_object(0,3));CHECK(image->header.flags&LV_IMAGE_FLAGS_COMPRESSED);CHECK(image->data[0]==1);
    image=lv_image_get_src(ui_get_object(0,4));lv_tick_inc(150);lv_timer_handler();CHECK(lv_image_get_src(ui_get_object(0,4))!=image);
    image=lv_image_get_src(ui_get_object(0,5));CHECK(image->header.cf==LV_COLOR_FORMAT_RAW_ALPHA);
    { int id;for(id=2;id<=5;++id){lv_image_decoder_dsc_t decoded;lv_result_t result;fprintf(stderr,"Decoding generated image %d (LZ4 %d, RLE %d)\n",id,LV_USE_LZ4,LV_USE_RLE);result=lv_image_decoder_open(&decoded,lv_image_get_src(ui_get_object(0,id)),NULL);if(result!=LV_RESULT_OK)fprintf(stderr,"Decoder %s cf %u stride %u\n",decoded.decoder?decoded.decoder->name:"none",decoded.header.cf,decoded.header.stride);CHECK(result==LV_RESULT_OK);CHECK(decoded.decoded!=NULL);lv_image_decoder_close(&decoded);} }
    ui_deinit();lv_display_delete(display);lv_deinit();puts("Generated assets and shared font references passed");return 0;
}
