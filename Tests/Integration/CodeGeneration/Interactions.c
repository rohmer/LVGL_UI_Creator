#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(value) do {if(!(value)){fprintf(stderr,"Interaction check failed at %d: %s (%s)\n",__LINE__,#value,ui_last_action_error());return 1;}}while(0)
int main(void) {
    lv_obj_t *source,*entry,*panel,*navigate,*return_button;
    lv_init();
#if LVGL_VERSION_MAJOR >= 9
    CHECK(lv_display_create(320,240));
#else
    lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=320;driver.ver_res=240;CHECK(lv_disp_drv_register(&driver));
#endif
    CHECK(ui_init());source=ui_get_object(0,7);entry=ui_get_object(0,9);panel=ui_get_object(0,10);navigate=ui_get_object(0,11);return_button=ui_get_object(1,20);
    CHECK(source&&entry&&panel&&navigate&&return_button);
    lv_obj_set_clickable(panel,true);
    lv_obj_send_event(source,LV_EVENT_CLICKED,NULL);
    CHECK(ui_variables.v_count==0);CHECK(lv_obj_has_state(entry,LV_STATE_USER_4));CHECK(!lv_obj_is_hidden(panel));CHECK(lv_obj_is_clickable(panel));
    lv_textarea_set_text(entry,"17");lv_obj_send_event(source,LV_EVENT_CLICKED,NULL);
    CHECK(ui_variables.v_count==2);CHECK(!lv_obj_has_state(entry,LV_STATE_USER_4));CHECK(lv_obj_is_hidden(panel));CHECK(lv_obj_is_clickable(panel));CHECK(!strcmp(ui_variables.v_caption,"After"));CHECK(ui_variables.v_temperature==0.75);
    lv_obj_send_event(source,LV_EVENT_READY,NULL);CHECK(!lv_obj_is_hidden(panel));CHECK(lv_obj_is_clickable(panel));
    lv_obj_send_event(source,LV_EVENT_READY,NULL);CHECK(lv_obj_is_hidden(panel));CHECK(lv_obj_is_clickable(panel));
    lv_obj_send_event(source,LV_EVENT_VALUE_CHANGED,NULL);CHECK(!strcmp(ui_variables.v_caption,"Compared"));
    CHECK(ui_set_v_enabled(false));lv_obj_send_event(source,LV_EVENT_CLICKED,NULL);CHECK(ui_variables.v_count==2);
    lv_obj_send_event(navigate,LV_EVENT_READY,NULL);CHECK(lv_screen_active()==ui_get_screen(1));lv_obj_send_event(return_button,LV_EVENT_CLICKED,NULL);CHECK(lv_screen_active()==ui_get_screen(0));CHECK(!lv_obj_is_hidden(panel));CHECK(lv_obj_is_clickable(panel));
    lv_obj_delete(panel);CHECK(ui_get_object(0,10)==NULL);lv_obj_send_event(source,LV_EVENT_CLICKED,NULL);CHECK(*ui_last_action_error());
    ui_deinit();CHECK(ui_init());CHECK(ui_variables.v_count==0);ui_deinit();
#if LVGL_VERSION_MAJOR >= 9
    lv_deinit();
#endif
    puts("Generated interaction callbacks passed");return 0;
}
