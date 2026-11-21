#include "ui.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(value) do { if (!(value)) { fprintf(stderr,"Variable binding check %d failed: %s\n",__LINE__,#value); return 1; } } while(0)
extern bool check_cpp_variable_bindings(void);
#if LVGL_VERSION_MAJOR >= 9
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels) { (void)area;(void)pixels;lv_display_flush_ready(display); }
#else
static void flush(lv_disp_drv_t *display,const lv_area_t *area,lv_color_t *pixels) { (void)area;(void)pixels;lv_disp_flush_ready(display); }
#endif
static void event(lv_obj_t *object) { lv_obj_send_event(object,LV_EVENT_VALUE_CHANGED,NULL); }
static bool reentrant_attempted,reentrant_result;
static void reentrant_caption(lv_event_t *event)
{
    (void)event;
    if(reentrant_attempted) return;
    reentrant_attempted=true;
    reentrant_result=ui_set_v_caption("Nested");
}
static bool all_levels(int32_t value)
{
    char text[32];snprintf(text,sizeof text,"%ld",(long)value);
    return ui_variables.v_level==value && lv_slider_get_value(ui_get_object(0,1))==value &&
        lv_bar_get_value(ui_get_object(0,2))==value && !strcmp(lv_label_get_text(ui_get_object(0,3)),text) &&
        lv_slider_get_value(ui_get_object(0,5))==value && lv_bar_get_value(ui_get_object(1,1))==value &&
        lv_slider_get_value(ui_get_object(1,2))==value;
}
int main(void)
{
    unsigned cycle;size_t count;int64_t entry;
    int32_t level=42,replacement=81;uint8_t small=8;bool enabled=true;float fraction=0.125f;
    char caption[16]="Application";char invalid[3]={'b','a','d'};
    int8_t signed8=INT8_MIN;int16_t signed16=INT16_MIN;uint16_t unsigned16=UINT16_MAX;uint32_t unsigned32=UINT32_MAX;
    int64_t timestamp=INT64_C(1770000042),legacy=31;double precise=0.125;
    lv_init();
#if LVGL_VERSION_MAJOR >= 9
    static uint8_t pixels[320*16*4];lv_display_t *display=lv_display_create(320,240);CHECK(display);
    lv_display_set_buffers(display,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
#else
    static lv_color_t pixels[320*16];static lv_disp_draw_buf_t draw;static lv_disp_drv_t driver;
    lv_disp_draw_buf_init(&draw,pixels,NULL,320*16);lv_disp_drv_init(&driver);driver.hor_res=320;driver.ver_res=240;driver.draw_buf=&draw;driver.flush_cb=flush;
    CHECK(lv_disp_drv_register(&driver));
#endif
    CHECK(!ui_bind_v_level(&level));CHECK(!ui_sync_variables());
    for(cycle=0;cycle<3;++cycle) {
        level=42;small=8;enabled=true;fraction=0.125f;strcpy(caption,"Application");
        CHECK(ui_init());CHECK(ui_init());CHECK(all_levels(12));CHECK(ui_history_v_level_count()==2);
        CHECK(lv_slider_get_value(ui_get_object(0,4))==7);
        CHECK(ui_bind_v_level(&level));CHECK(level==42);CHECK(all_levels(42));CHECK(ui_history_v_level_count()==3);
        count=ui_history_v_level_count();CHECK(ui_sync_v_level());CHECK(ui_history_v_level_count()==count);
        CHECK(!ui_bind_v_level(NULL));CHECK(ui_set_v_level(43));CHECK(level==43);
        lv_slider_set_value(ui_get_object(0,1),55,LV_ANIM_OFF);event(ui_get_object(0,1));CHECK(level==55);CHECK(all_levels(55));
        CHECK(lv_slider_get_value(ui_get_object(0,4))==7);
        lv_slider_set_value(ui_get_object(0,4),22,LV_ANIM_OFF);event(ui_get_object(0,4));CHECK(level==22);CHECK(all_levels(22));
        lv_slider_set_value(ui_get_object(0,5),99,LV_ANIM_OFF);event(ui_get_object(0,5));CHECK(level==22);CHECK(ui_variables.v_level==22);
        level=63;CHECK(ui_sync_v_level());CHECK(all_levels(63));CHECK(lv_slider_get_value(ui_get_object(0,4))==22);
        CHECK(ui_show_screen(1));lv_slider_set_value(ui_get_object(1,2),44,LV_ANIM_OFF);event(ui_get_object(1,2));CHECK(level==44);CHECK(all_levels(44));
        lv_obj_send_event(ui_get_object(1,7),LV_EVENT_CLICKED,NULL);CHECK(level==64);CHECK(all_levels(64));
        lv_obj_send_event(ui_get_object(1,8),LV_EVENT_CLICKED,NULL);CHECK(level==65);CHECK(all_levels(65));
        CHECK(ui_history_v_level_get(ui_history_v_level_count()-1,&entry));CHECK(entry==65);
        CHECK(!ui_set_v_level(INT64_MAX));CHECK(level==65);CHECK(all_levels(65));
        CHECK(ui_bind_v_level(&replacement));CHECK(all_levels(81));CHECK(ui_set_v_level(82));CHECK(replacement==82);CHECK(level==65);
        ui_unbind_v_level();CHECK(ui_set_v_level(50));CHECK(replacement==82);CHECK(all_levels(50));replacement=81;

        CHECK(ui_bind_v_caption(caption,sizeof caption));CHECK(!strcmp(caption,"Application"));
        CHECK(!strcmp(lv_textarea_get_text(ui_get_object(0,6)),caption));CHECK(!strcmp(lv_label_get_text(ui_get_object(1,3)),caption));
        reentrant_attempted=false;reentrant_result=true;
        lv_obj_add_event_cb(ui_get_object(0,6),reentrant_caption,LV_EVENT_VALUE_CHANGED,NULL);
        CHECK(ui_set_v_enabled(true));CHECK(ui_set_v_caption("Outer"));
        CHECK(reentrant_attempted);CHECK(!reentrant_result);CHECK(!strcmp(caption,"Outer"));CHECK(!strcmp(ui_variables.v_caption,"Outer"));
        CHECK(!strcmp(lv_textarea_get_text(ui_get_object(0,6)),"Outer"));CHECK(!strcmp(lv_label_get_text(ui_get_object(0,7)),"Outer"));CHECK(!strcmp(lv_label_get_text(ui_get_object(1,3)),"Outer"));
        lv_obj_remove_event_cb(ui_get_object(0,6),reentrant_caption);CHECK(ui_set_v_enabled(false));
        lv_textarea_set_text(ui_get_object(0,6),"Typed");CHECK(!strcmp(caption,"Typed"));CHECK(!strcmp(lv_label_get_text(ui_get_object(0,7)),"Typed"));
        count=ui_history_v_caption_count();CHECK(ui_sync_v_caption());CHECK(ui_history_v_caption_count()==count);
        CHECK(ui_set_v_caption(ui_variables.v_caption));CHECK(!strcmp(caption,"Typed"));
        count=ui_history_v_caption_count();CHECK(!ui_set_v_caption("This does not fit the buffer"));CHECK(ui_history_v_caption_count()==count);CHECK(!strcmp(caption,"Typed"));
        lv_textarea_set_text(ui_get_object(0,6),"This does not fit the buffer");CHECK(!strcmp(caption,"Typed"));CHECK(!strcmp(lv_textarea_get_text(ui_get_object(0,6)),"Typed"));
        CHECK(!ui_bind_v_caption(invalid,sizeof invalid));CHECK(ui_set_v_caption("Still bound"));CHECK(!strcmp(caption,"Still bound"));
        strcpy(caption,"Device");CHECK(ui_sync_v_caption());CHECK(!strcmp(lv_label_get_text(ui_get_object(1,3)),"Device"));

        CHECK(ui_bind_v_enabled(&enabled));CHECK(lv_obj_has_state(ui_get_object(0,9),LV_STATE_CHECKED));CHECK(lv_obj_has_state(ui_get_object(0,10),LV_STATE_CHECKED));
        lv_obj_remove_state(ui_get_object(0,15),LV_STATE_CHECKED);event(ui_get_object(0,15));CHECK(enabled);
        lv_obj_remove_state(ui_get_object(0,13),LV_STATE_CHECKED);event(ui_get_object(0,13));CHECK(!enabled);CHECK(!lv_obj_has_state(ui_get_object(0,9),LV_STATE_CHECKED));
        lv_obj_add_state(ui_get_object(0,14),LV_STATE_CHECKED);event(ui_get_object(0,14));CHECK(enabled);CHECK(lv_obj_has_state(ui_get_object(0,10),LV_STATE_CHECKED));
        lv_obj_remove_state(ui_get_object(0,9),LV_STATE_CHECKED);event(ui_get_object(0,9));CHECK(!enabled);CHECK(!lv_obj_has_state(ui_get_object(0,10),LV_STATE_CHECKED));CHECK(!strcmp(lv_label_get_text(ui_get_object(0,11)),"false"));
        CHECK(ui_bind_v_fraction(&fraction));CHECK(ui_set_v_fraction(0.1));CHECK(fraction==0.1f);CHECK(ui_variables.v_fraction==(double)fraction);
        count=ui_history_v_fraction_count();CHECK(ui_sync_v_fraction());CHECK(ui_history_v_fraction_count()==count);
        CHECK(!ui_set_v_fraction(DBL_MAX));CHECK(!ui_set_v_fraction(NAN));CHECK(!ui_set_v_fraction(INFINITY));
        fraction=0.75f;CHECK(ui_sync_v_fraction());CHECK(ui_variables.v_fraction==0.75);
        CHECK(ui_bind_v_small(&small));CHECK(!ui_set_v_small(-1));CHECK(!ui_set_v_small(256));CHECK(small==8);
        lv_spinbox_set_value(ui_get_object(0,12),300);event(ui_get_object(0,12));CHECK(small==8);CHECK(lv_spinbox_get_value(ui_get_object(0,12))==8);
        CHECK(ui_set_v_small(255));CHECK(small==255);CHECK(!strcmp(lv_label_get_text(ui_get_object(1,6)),"255"));
        lv_obj_send_event(ui_get_object(1,9),LV_EVENT_CLICKED,NULL);CHECK(small==254);CHECK(ui_variables.v_small==254);

        CHECK(ui_bind_v_signed8(&signed8));CHECK(ui_variables.v_signed8==INT8_MIN);CHECK(!ui_set_v_signed8(128));
        CHECK(ui_bind_v_signed16(&signed16));CHECK(ui_variables.v_signed16==INT16_MIN);CHECK(!ui_set_v_signed16(32768));
        CHECK(ui_bind_v_unsigned16(&unsigned16));CHECK(ui_variables.v_unsigned16==UINT16_MAX);CHECK(!ui_set_v_unsigned16(-1));
        CHECK(ui_bind_v_unsigned32(&unsigned32));CHECK(ui_variables.v_unsigned32==UINT32_MAX);CHECK(!ui_set_v_unsigned32(INT64_MAX));
        CHECK(ui_bind_v_timestamp(&timestamp));CHECK(ui_variables.v_timestamp==timestamp);
        CHECK(ui_bind_v_double(&precise));CHECK(ui_variables.v_double==precise);
        CHECK(ui_set_v_legacy_label(10));CHECK(ui_set_v_legacy_label_14(20));CHECK(ui_variables.v_legacy_label==10);CHECK(ui_variables.v_legacy_label_14==20);
        CHECK(ui_bind_v_legacy(&legacy));CHECK(lv_bar_get_value(ui_get_object(1,5))==31);
        lv_bar_set_value(ui_get_object(1,5),32,LV_ANIM_OFF);event(ui_get_object(1,5));CHECK(legacy==32);
        CHECK(ui_sync_variables());CHECK(check_cpp_variable_bindings());

        lv_obj_delete(ui_get_object(0,2));CHECK(!ui_get_object(0,2));CHECK(ui_set_v_level(70));
        ui_deinit();CHECK(!ui_sync_variables());CHECK(!ui_set_v_level(1));CHECK(caption[0]);
        level=91;CHECK(ui_init());CHECK(ui_set_v_level(12));CHECK(level==91);CHECK(ui_history_v_caption_count()==0);ui_deinit();
        legacy=31;
    }
#if LVGL_VERSION_MAJOR >= 9
    lv_deinit();
#endif
    puts("C11/C++ typed application variables, shared widgets, directions, strings, history and lifecycle passed");return 0;
}
