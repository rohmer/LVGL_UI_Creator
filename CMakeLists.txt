cmake_minimum_required(VERSION 3.0.0)

project(LVGL_UI_Creator)

# 3rdParty Libraries
set(BUILD_SHARED_LIBS true)
add_subdirectory(3rdParty/SDL2-2.0.10)
add_subdirectory(3rdParty/LVGL)
add_subdirectory(3rdParty/freetype-2.10.1)
add_subdirectory(3rdParty/libimagequant)

# 1st Party libs
add_subdirectory(LVSerial)
add_subdirectory(ImgLib)

# UICreator App
add_subdirectory(UICreator)