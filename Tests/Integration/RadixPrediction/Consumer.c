#include "ui.h"
#include "lv_advanced_keyboard.h"
#include "lv_advanced_keyboard_prediction.h"
#include <stdio.h>
#include <string.h>
int radix_cpp_header_consumer(void);
#define CHECK(good) do {if(!(good)){fprintf(stderr,"Radix C consumer line %d: %s\n",__LINE__,#good);return 1;}}while(0)
int main(void) {
    unsigned cycle;lv_init();lv_display_t* display=lv_display_create(640,400);CHECK(display);
    CHECK(radix_cpp_header_consumer());
    for(cycle=0;cycle!=3;++cycle) {
        lv_advanced_keyboard_suggestion_t words[3];
        CHECK(ui_init());lv_obj_t* keyboard=ui_get_object(0,7);CHECK(keyboard);
        lv_advanced_keyboard_predictor_t* predictor=lv_advanced_keyboard_get_predictor(keyboard);
        CHECK(predictor&&lv_advanced_keyboard_predictor_node_count(predictor)==12);
        CHECK(lv_advanced_keyboard_predictor_suggest_folded(predictor,"h",1,NULL,0,NULL,0,words,3)==1);
        CHECK(words[0].length==5&&!memcmp(words[0].text,"hello",5));ui_deinit();
#if RADIX_EXPECT_MASS
        CHECK(words[0].mass==RADIX_EXPECT_MASS);
#endif
    }
    lv_display_delete(display);lv_deinit();puts("Generated C11 loads embedded legacy/radix/smoothed models without desktop encoder");return 0;
}
