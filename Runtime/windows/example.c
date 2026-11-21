#include "lvgl_pb_display.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

static int display_project(const char* utf8_path)
{
    char error[512];
    lvgl_pb_window_t* window=lvgl_pb_window_create(320,240,"Protobuf display example",error,sizeof error);
    if(!window){fprintf(stderr,"Display initialization failed: %s\n",error);return 1;}
    if(!lvgl_pb_window_load_file(window,utf8_path)) {
        fprintf(stderr,"Project load failed: %s\n",lvgl_pb_window_error(window));
        lvgl_pb_window_destroy(window);return 1;
    }
    printf("Loaded %zu screens and %zu variables\n",lvgl_pb_window_screen_count(window),
        lvgl_pb_window_variable_count(window));
    for(size_t i=0;i<lvgl_pb_window_variable_count(window);++i)
        printf("Variable: %s\n",lvgl_pb_window_variable_name(window,i));
    /* All library calls stay on this UI thread. Application code can set
     * variables or switch screens between polls. The DLL handles mouse/keyboard. */
    while(lvgl_pb_window_poll(window)) { }
    lvgl_pb_window_destroy(window);
    return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
{
    if(argc!=2){fprintf(stderr,"Usage: LVGLProtobufExample project.pb\n");return 2;}
    const int required=WideCharToMultiByte(CP_UTF8,0,argv[1],-1,NULL,0,NULL,NULL);
    if(required<=0)return 2;
    char* utf8=(char*)malloc((size_t)required);
    if(!utf8)return 1;
    if(!WideCharToMultiByte(CP_UTF8,0,argv[1],-1,utf8,required,NULL,NULL)){free(utf8);return 2;}
    const int result=display_project(utf8);
    free(utf8);return result;
}
#else
int main(int argc,char** argv)
{
    if(argc!=2){fprintf(stderr,"Usage: LVGLProtobufExample project.pb\n");return 2;}
    return display_project(argv[1]);
}
#endif
