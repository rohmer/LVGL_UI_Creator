cmake_minimum_required(VERSION 3.0.0)

file(GLOB_RECURSE ImgLib_SRC
    "*.h"
    "*.cpp")

add_library(ImgLib ${ImgLib_SRC} )