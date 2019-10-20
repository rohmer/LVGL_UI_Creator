cmake_minimum_required(VERSION 3.0.0)

file(GLOB_RECURSE LVSerial_SRC
    "*.h"
    "*.cpp")

add_library(LVSerial ${LVSerial_SRC} )