#include "ui.h"
#include <stdio.h>
#define CHECK(value) do {if(!(value)){fprintf(stderr,"Screen/focus check failed at %d: %s\n",__LINE__,#value);return 1;}}while(0)
#if LVGL_VERSION_MAJOR == 8
static void flush(lv_disp_drv_t *display,const lv_area_t *area,lv_color_t *pixels){(void)area;(void)pixels;lv_disp_flush_ready(display);}
#else
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels){(void)area;(void)pixels;lv_display_flush_ready(display);}
#endif
static void advance(unsigned duration) {unsigned i;for(i=0;i<duration;i+=5){lv_tick_inc(5);lv_timer_handler();}}
static int excluded(lv_obj_t *object) {
    uint32_t i;if(lv_obj_get_group(object))return 0;
    for(i=0;i<lv_obj_get_child_count(object);++i)if(!excluded(lv_obj_get_child(object,i)))return 0;
    return 1;
}
int main(void) {
    lv_group_t *group;lv_obj_t *previous;unsigned cycle;
    lv_init();
#if LVGL_VERSION_MAJOR >= 9
    static uint8_t pixels[320*16*4];lv_display_t *display=lv_display_create(320,240);CHECK(display);
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
#else
    static lv_color_t pixels[320*16];static lv_disp_draw_buf_t draw;static lv_disp_drv_t driver;
    lv_disp_draw_buf_init(&draw,pixels,NULL,320*16);lv_disp_drv_init(&driver);driver.hor_res=320;driver.ver_res=240;driver.draw_buf=&draw;driver.flush_cb=flush;CHECK(lv_disp_drv_register(&driver));
#endif
    group=lv_group_create();CHECK(group);lv_group_set_default(group);previous=lv_screen_active();
    for(cycle=0;cycle<12;++cycle) {
        CHECK(ui_init());CHECK(ui_init());
        CHECK(lv_group_get_obj_count(group)==3);
        CHECK(lv_group_get_focused(group)==ui_get_object(0,1));
        lv_group_focus_obj(ui_get_object(0,2));
        lv_group_focus_next(group);CHECK(lv_group_get_focused(group)==ui_get_object(0,1));
        lv_group_focus_next(group);CHECK(lv_group_get_focused(group)==ui_get_object(0,4));
        CHECK(excluded(ui_get_object(0,3)));CHECK(!lv_obj_get_group(ui_get_object(1,5)));
        if(cycle==0) {ui_deinit();advance(120);CHECK(lv_screen_active()==previous);CHECK(lv_group_get_obj_count(group)==0);continue;}
        advance(120);CHECK(lv_screen_active()==ui_get_screen(0));
        CHECK(ui_show_screen(1));CHECK(lv_group_get_obj_count(group)==2);
        CHECK(lv_group_get_focused(group)==ui_get_object(1,6));
        CHECK(!lv_obj_get_group(ui_get_object(0,1)));
        advance(40);lv_obj_update_layout(ui_get_screen(1));
        CHECK(lv_obj_get_x(ui_get_screen(1))>0);CHECK(lv_obj_get_x(ui_get_screen(1))<320);
        advance(100);lv_obj_update_layout(ui_get_screen(1));CHECK(lv_screen_active()==ui_get_screen(1));CHECK(lv_obj_get_x(ui_get_screen(1))==0);
        lv_obj_delete(ui_get_object(1,6));CHECK(!ui_get_object(1,6));CHECK(lv_group_get_obj_count(group)==1);
        CHECK(ui_show_screen(0));CHECK(lv_group_get_obj_count(group)==3);
        CHECK(lv_group_get_focused(group)==ui_get_object(0,2));
        ui_deinit();advance(120);CHECK(lv_screen_active()==previous);CHECK(lv_group_get_obj_count(group)==0);
    }
    lv_group_set_default(NULL);lv_group_delete(group);
#if LVGL_VERSION_MAJOR >= 9
    lv_deinit();
#endif
    puts("Generated screen transitions, active focus order, internal exclusions and lifecycle passed");return 0;
}
