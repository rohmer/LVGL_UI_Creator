#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv) {
    int x11=0,wayland=0,dummy=0;
    for(int index=0;index<SDL_GetNumVideoDrivers();++index) {
        const char* name=SDL_GetVideoDriver(index);puts(name);
        x11|=strcmp(name,"x11")==0;wayland|=strcmp(name,"wayland")==0;dummy|=strcmp(name,"dummy")==0;
    }
    for(int i=1;i<argc;++i) {
        if(strcmp(argv[i],"--require-x11")==0&&!x11)return 2;
        if(strcmp(argv[i],"--require-wayland")==0&&!wayland)return 3;
    }
    return dummy?0:4;
}
