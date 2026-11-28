#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { fprintf(stderr,"Advanced controls C consumer line %d: %s\n",__LINE__,#test); return 1; } } while (0)
extern bool advanced_controls_cpp_api(lv_obj_t *radio);
static unsigned changes;
static void changed(lv_event_t *event) { (void)event; ++changes; }
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels)
{ (void)area; (void)pixels; lv_display_flush_ready(display); }

int main(void)
{
    unsigned pass;
    void *pixels;
    lv_display_t *display;
    lv_obj_t *original;
    lv_init();
    display=lv_display_create(800,600); CHECK(display);
    pixels=malloc(800*20*4); CHECK(pixels);
    lv_display_set_buffers(display,pixels,NULL,800*20*4,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    original=lv_screen_active();
    for (pass=0;pass<3;++pass) {
        lv_obj_t *radio,*checkbox,*nested,*date,*menu,*popup;
        CHECK(ui_init()); CHECK(ui_init());
        radio=ui_get_object(0,1); checkbox=ui_get_object(0,2); nested=ui_get_object(0,3);
        date=ui_get_object(0,4); menu=ui_get_object(0,5);
        CHECK(radio && checkbox && nested && date && menu);
        changes=0;
        lv_obj_add_event_cb(radio,changed,LV_EVENT_VALUE_CHANGED,NULL);
        CHECK(ui_advanced_get_selected(radio)==0);
        CHECK(ui_variables.v_radioSelection==0);
        CHECK(!ui_advanced_set_selected(radio,2));
        CHECK(ui_advanced_get_selected(radio)==0);
        lv_obj_send_event(ui_advanced_part(radio,"Option1"),LV_EVENT_CLICKED,NULL);
        CHECK(ui_advanced_get_selected(radio)==1 && changes==1);
        CHECK(ui_variables.v_radioSelection==1);
        CHECK(!ui_set_v_radioSelection(2)); CHECK(ui_variables.v_radioSelection==1);
        CHECK(advanced_controls_cpp_api(radio));
        CHECK(!ui_advanced_get_checked(checkbox)); CHECK(ui_advanced_get_checked(nested));
        lv_obj_send_event(checkbox,LV_EVENT_CLICKED,NULL);
        CHECK(ui_advanced_get_checked(checkbox));
        CHECK(ui_variables.v_checkboxValue);
        CHECK(ui_set_v_nestedValue(false)); CHECK(!ui_advanced_get_checked(nested));
        CHECK(ui_advanced_set_checked(nested,false)); CHECK(!ui_advanced_get_checked(nested));
        CHECK(!lv_obj_has_state(ui_advanced_part(nested,"Checkbox"),LV_STATE_CHECKED));
        CHECK(strcmp(ui_advanced_get_date(date),"2028-02-29")==0);
        CHECK(!ui_advanced_set_date(date,"2028-02-30"));
        CHECK(!ui_advanced_set_date(date,"2029-01-01"));
        CHECK(!ui_set_v_chosenDate("2028-02-30"));
        CHECK(!ui_set_v_chosenDate("2029-01-01"));
        CHECK(strcmp(ui_variables.v_chosenDate,"2028-02-29")==0);
        CHECK(strcmp(ui_advanced_get_date(date),"2028-02-29")==0);
        CHECK(ui_advanced_open_date(date));
        popup=ui_advanced_part(date,"Popup"); CHECK(popup && !lv_obj_is_hidden(popup));
        CHECK(ui_advanced_part(date,"Calendar"));
        CHECK(ui_advanced_close_date(date)); CHECK(lv_obj_is_hidden(popup));
        CHECK(ui_set_v_chosenDate("2028-03-15"));
        CHECK(strcmp(ui_advanced_get_date(date),"2028-03-15")==0);
        CHECK(ui_advanced_get_selected(menu)==1);
        lv_obj_send_event(ui_advanced_part(menu,"Item2"),LV_EVENT_CLICKED,NULL);
        CHECK(!lv_obj_is_hidden(ui_advanced_part(menu,"Item3")));
        CHECK(!ui_advanced_set_selected(menu,4));
        lv_obj_send_event(ui_advanced_part(menu,"Item3"),LV_EVENT_CLICKED,NULL);
        CHECK(ui_advanced_get_selected(menu)==3);
        CHECK(ui_variables.v_menuSelection==3);
        CHECK(!ui_set_v_menuSelection(4)); CHECK(ui_variables.v_menuSelection==3);
        lv_obj_send_event(ui_advanced_part(menu,"Back"),LV_EVENT_CLICKED,NULL);
        CHECK(lv_obj_is_hidden(ui_advanced_part(menu,"Item3")));
        lv_refr_now(display);
        ui_deinit(); CHECK(lv_screen_active()==original); ui_deinit();
    }
    lv_display_delete(display); lv_deinit(); free(pixels);
    puts("Five advanced controls exported as standalone C, exercised through a C/C++ public API across three lifetimes");
    return 0;
}
