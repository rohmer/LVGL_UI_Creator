#include "lvgl_pb_board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned frames;
static void flush(lvgl_pb_board_t* board,int32_t x1,int32_t y1,int32_t x2,int32_t y2,
                  const void* pixels,size_t bytes,void* context)
{
    (void)x1;(void)y1;(void)x2;(void)y2;(void)context;
    if(pixels && bytes)++frames;
    lvgl_pb_board_flush_ready(board);
}
static void click(lvgl_pb_board_t* board,int x,int y)
{
    lvgl_pb_board_pointer(board,x,y,false);
    lvgl_pb_board_pointer(board,x,y,true);
    lvgl_pb_board_pointer(board,x,y,false);
}
int main(int argc,char** argv)
{
    if(argc!=2)return 1;
    FILE* file=fopen(argv[1],"rb");if(!file)return 2;
    if(fseek(file,0,SEEK_END))return 3;
    long length=ftell(file);if(length<=0)return 4;
    rewind(file);unsigned char* bytes=(unsigned char*)malloc((size_t)length);
    if(!bytes || fread(bytes,1,(size_t)length,file)!=(size_t)length)return 5;
    fclose(file);
    char error[512];lvgl_pb_board_t* board=lvgl_pb_board_create(640,400,32,flush,NULL,error,sizeof error);
    if(!board){fprintf(stderr,"%s\n",error);return 6;}
    if(!lvgl_pb_board_load_memory(board,bytes,(size_t)length)) {
        fprintf(stderr,"%s\n",lvgl_pb_board_error(board));return 7;
    }
    free(bytes);
    if(!lvgl_pb_board_object(board,0,0) || !lvgl_pb_board_object(board,0,1))return 8;
    lvgl_pb_board_poll(board,40);lvgl_pb_value_t value={0};
    if(!lvgl_pb_board_get_variable(board,"count",&value) || value.integer_value!=0)return 9;
    click(board,80,100);
    lvgl_pb_board_poll(board,40);
    if(!lvgl_pb_board_get_variable(board,"count",&value) || value.integer_value!=1) {
        fprintf(stderr,"count=%lld error=%s\n",(long long)value.integer_value,lvgl_pb_board_error(board));return 10;
    }
    value.type=LVGL_PB_STRING;value.string_value="";
    if(!lvgl_pb_board_set_variable(board,"message",&value))return 12;
    click(board,30,30);click(board,280,190);click(board,160,190);click(board,370,238);
    if(!lvgl_pb_board_get_variable(board,"message",&value) || strcmp(value.string_value,"teh")) {
        fprintf(stderr,"Typed text: %s\n",value.string_value?value.string_value:"(null)");return 13;
    }
    click(board,310,335);
    if(!lvgl_pb_board_get_variable(board,"message",&value) || strcmp(value.string_value,"the ")) {
        fprintf(stderr,"Corrected text: %s\n",value.string_value?value.string_value:"(null)");return 14;
    }
    lvgl_pb_board_destroy(board);
    if(!frames)return 11;
    puts("Generated Raspberry Pi Linux C ABI rendered the screen, executed its click action, typed into Advanced Keyboard and autocorrected teh to the");
    return 0;
}
