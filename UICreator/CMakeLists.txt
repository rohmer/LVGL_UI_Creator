cmake_minimum_required(VERSION 3.0.0)

file(GLOB_RECURSE UICreator_SRC
    "*.h"
    "*.cpp"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(../3rdParty/LVGL)
include_directories(../3rdParty/LVGL/lvgl)
include_directories(../3rdParty/freetype-2.10.1/include)
include_directories(../LVSerial)
add_executable(UICreator ${UICreator_SRC})