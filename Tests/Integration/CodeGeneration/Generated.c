#include "ui.h"
#include "fixture_ids.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x); return 1; } } while (0)
#define OBJ(name) ui_get_object(0, ID_##name)
int main(void) {
    lv_display_t *display; lv_obj_t *previous; int64_t history;
    lv_init();display=lv_display_create(800,600);previous=lv_screen_active();
    CHECK(ui_init());CHECK(ui_init());CHECK(ui_screen_count()==2);
    CHECK(lv_screen_active()==ui_get_screen(0));
    CHECK(lv_obj_get_x(ui_get_screen(0))==0&&lv_obj_get_y(ui_get_screen(0))==0);
    CHECK(lv_obj_get_class(OBJ(checkbox))==&lv_checkbox_class);CHECK(lv_obj_is_checked(OBJ(checkbox)));
    CHECK(lv_arc_get_value(OBJ(arc))==42);CHECK(lv_bar_get_value(OBJ(bar))==51);
    CHECK(lv_slider_get_value(OBJ(slider))==75);CHECK(lv_spinbox_get_value(OBJ(spinbox))==123);
    CHECK(lv_dropdown_get_selected(OBJ(dropdown))==2);CHECK(lv_roller_get_selected(OBJ(roller))==1);
    CHECK(lv_led_get_brightness(OBJ(led))==127);CHECK(lv_obj_get_style_border_width(OBJ(arc),0)==3);
    CHECK(strcmp(lv_label_get_text(OBJ(label)),"Text \"quoted\" \\ newline\n UTF-8 \xc3\xa9")==0);
    CHECK(lv_tabview_get_tab_active(OBJ(tabview))==1);
    CHECK(lv_obj_get_parent(ui_get_object(0,100))==lv_obj_get_child(lv_tabview_get_content(OBJ(tabview)),0));
    CHECK(lv_chart_get_series_y_array(OBJ(chart),lv_chart_get_series_next(OBJ(chart),NULL))[2]==20);
    CHECK(strcmp(lv_table_get_cell_value(OBJ(table),1,1),"Cell")==0);
    CHECK(lv_image_get_src(OBJ(image))!=NULL);CHECK(lv_canvas_get_draw_buf(OBJ(canvas))!=NULL);
    CHECK(lv_imagebutton_get_src_middle(OBJ(imagebutton),LV_IMAGEBUTTON_STATE_RELEASED)!=NULL);
    CHECK(ui_history_v_level_count()==2);CHECK(ui_history_v_level_get(0,&history)&&history==50);
    CHECK(strcmp(lv_textarea_get_text(OBJ(textarea)),"Bound caption")==0);
    CHECK(ui_set_v_level(60));CHECK(ui_set_v_level(70));CHECK(ui_set_v_level(80));CHECK(lv_bar_get_value(OBJ(bar))==80);
    CHECK(ui_history_v_level_count()==2);CHECK(ui_history_v_level_get(0,&history)&&history==70);
    lv_bar_set_value(OBJ(bar),22,LV_ANIM_OFF);lv_obj_send_event(OBJ(bar),LV_EVENT_VALUE_CHANGED,NULL);CHECK(ui_variables.v_level==22);
    CHECK(ui_set_v_caption("Device text"));CHECK(strcmp(lv_textarea_get_text(OBJ(textarea)),"Device text")==0);
    lv_textarea_set_text(OBJ(textarea),"Typed");CHECK(strcmp(ui_variables.v_caption,"Typed")==0);
    CHECK(ui_set_v_enabled(false));CHECK(!lv_obj_is_checked(OBJ(switch)));
    CHECK(ui_set_v_fraction(0.5));CHECK(ui_variables.v_fraction==0.5);
    CHECK(ui_set_v_timestamp(INT64_C(1770000001)));CHECK(ui_variables.v_timestamp==INT64_C(1770000001));
    lv_obj_send_event(lv_obj_get_child(OBJ(textBox),3),LV_EVENT_CLICKED,NULL);CHECK(ui_variables.v_custom_level==70);
    CHECK(ui_set_v_custom_level(80));CHECK(strcmp(lv_textarea_get_text(lv_obj_get_child(OBJ(textBox),1)),"80")==0);
    { lv_obj_t *content=lv_obj_get_child(OBJ(layerView),1);lv_obj_send_event(lv_obj_get_child(content,0),LV_EVENT_CLICKED,NULL);CHECK(ui_variables.v_custom_selected==1); }
    CHECK(ui_set_v_caption(ui_variables.v_caption));CHECK(strcmp(lv_textarea_get_text(OBJ(textarea)),"Typed")==0);
    CHECK(lv_line_get_point_count(OBJ(line))==3);CHECK(lv_line_get_y_invert(OBJ(line)));
    CHECK(lv_spinner_get_arc_sweep(OBJ(spinner))==80);CHECK(lv_keyboard_get_mode(OBJ(keyboard))==LV_KEYBOARD_MODE_NUMBER);
    { const void *frame=lv_image_get_src(OBJ(image));lv_tick_inc(150);lv_timer_handler();CHECK(lv_image_get_src(OBJ(image))!=frame); }
    CHECK(ui_show_screen(1));CHECK(lv_obj_is_checked(ui_get_object(1,1)));CHECK(!ui_show_screen(99));
    lv_obj_delete(OBJ(arc));CHECK(OBJ(arc)==NULL);
    ui_deinit();CHECK(lv_screen_active()==previous);CHECK(ui_get_screen(0)==NULL);CHECK(OBJ(bar)==NULL);CHECK(ui_history_v_level_count()==0);CHECK(!ui_set_v_level(1));
    { int i; for(i=0;i<5;++i) { CHECK(ui_init());ui_deinit();CHECK(ui_get_screen(0)==NULL); } } ui_deinit();
    lv_display_delete(display);lv_deinit();puts("Generated C runtime passed");return 0;
}
