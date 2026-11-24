#include "ui.h"
#include "lv_advanced_keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(value) do { if(!(value)){fprintf(stderr,"Advanced Keyboard C consumer line %d: %s\n",__LINE__,#value);return 1;} } while(0)
static void Flush(lv_display_t* display,const lv_area_t* area,uint8_t* data) {(void)area;(void)data;lv_display_flush_ready(display);}
int main(void) {
    unsigned pass;lv_init();
    lv_display_t* display=lv_display_create(640,400);CHECK(display);
    void* pixels=malloc(640*20*4);CHECK(pixels);
    lv_display_set_buffers(display,pixels,NULL,640*20*4,LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,Flush);
    lv_obj_t* original=lv_screen_active();
    for(pass=0;pass<3;++pass) {
        CHECK(ui_init());CHECK(ui_init());
        lv_obj_t* keyboard=ui_get_object(0,7);CHECK(keyboard && lv_advanced_keyboard_is_instance(keyboard));
        CHECK(strcmp(lv_advanced_keyboard_get_language(keyboard),"fr-FR")==0);
        lv_obj_t* keys=lv_advanced_keyboard_get_keyboard(keyboard);CHECK(keys);
        CHECK(strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(keys,0),0)),"a")==0);
        CHECK(strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(keys,1),0)),"z")==0);
        lv_obj_t* entry=ui_get_object(0,8);CHECK(entry && lv_obj_has_class(entry,&lv_textarea_class));
        CHECK(lv_obj_get_child_count(keyboard)==2);
        lv_obj_send_event(entry,LV_EVENT_FOCUSED,NULL);CHECK(lv_advanced_keyboard_get_textarea(keyboard)==entry);
        CHECK(!lv_obj_is_hidden(keyboard));
        CHECK(strcmp(lv_advanced_keyboard_get_text(keyboard),"Hello")==0);
        CHECK(lv_textarea_get_max_length(entry)==64);
        CHECK(strcmp(lv_textarea_get_placeholder_text(entry),"Type a message")==0);
        CHECK(ui_set_v_message("C export"));CHECK(strcmp(lv_advanced_keyboard_get_text(keyboard),"C export")==0);
        lv_textarea_set_text(lv_advanced_keyboard_get_textarea(keyboard),"Typed");
        CHECK(strcmp(ui_variables.v_message,"Typed")==0);
        lv_refr_now(display);ui_deinit();CHECK(lv_screen_active()==original);ui_deinit();
    }
    lv_display_delete(display);lv_deinit();free(pixels);
    puts("Standalone Advanced Keyboard support compiled with generated C11, rendered and updated two-way bindings across three lifetimes");return 0;
}
