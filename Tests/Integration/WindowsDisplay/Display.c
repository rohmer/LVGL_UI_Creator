#include "lvgl_pb_display.h"
#include <stdio.h>
#include <stdlib.h>
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char** argv) {
    unsigned iteration;char error[512];
    REQUIRE(argc==2);
    for(iteration=0;iteration<3;++iteration) {
        fprintf(stderr,"Display lifetime %u: create\n",iteration);
        lvgl_pb_window_t* window=lvgl_pb_window_create(320,240,"Runtime test",error,sizeof error);
        REQUIRE(window!=NULL);
        REQUIRE(lvgl_pb_window_create(320,240,"Second window",error,sizeof error)==NULL);
        REQUIRE(lvgl_pb_window_load_file(window,argv[1]));
        REQUIRE(lvgl_pb_window_screen_count(window)==2);
        REQUIRE(lvgl_pb_window_show_screen(window,1));
        REQUIRE(!lvgl_pb_window_show_screen(window,2));
        REQUIRE(lvgl_pb_window_variable_count(window)>0);
        {
            lvgl_pb_value_t value;
            const char* name=lvgl_pb_window_variable_name(window,0);
            REQUIRE(name!=NULL);REQUIRE(lvgl_pb_window_get_variable(window,name,&value));
            REQUIRE(lvgl_pb_window_set_variable(window,name,&value));
        }
        fprintf(stderr,"Display lifetime %u: invalid load\n",iteration);
        REQUIRE(!lvgl_pb_window_load_memory(window,"invalid",7));
        REQUIRE(lvgl_pb_window_screen_count(window)==2); /* Rollback preserves the old project. */
        fprintf(stderr,"Display lifetime %u: reload\n",iteration);
        REQUIRE(lvgl_pb_window_load_file(window,argv[1]));
        REQUIRE(lvgl_pb_window_poll(window));
        fprintf(stderr,"Display lifetime %u: destroy\n",iteration);
        lvgl_pb_window_destroy(window);
    }
    lvgl_pb_window_destroy(NULL);
    REQUIRE(lvgl_pb_window_create(0,240,"Bad dimensions",error,sizeof error)==NULL);
    puts("Windows display C ABI, file loading, variables, rollback, repeated lifetime passed");return 0;
}
