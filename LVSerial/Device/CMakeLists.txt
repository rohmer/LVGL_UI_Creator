cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LVSerialDevice LANGUAGES C)
endif()
get_filename_component(repository_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_library(LVSerialDevice STATIC lvserial_pb.c lvserial_defaults.c ../Proto/lvserial.pb.c
    "${repository_root}/3rdParty/nanopb/pb_common.c"
    "${repository_root}/3rdParty/nanopb/pb_encode.c"
    "${repository_root}/3rdParty/nanopb/pb_decode.c")
target_include_directories(LVSerialDevice PUBLIC "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../Proto" "${repository_root}/3rdParty/nanopb")
set_target_properties(LVSerialDevice PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
