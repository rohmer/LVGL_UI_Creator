cmake_minimum_required(VERSION 3.24)
project(RadixModelConverter LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/AdvancedKeyboardDependency.cmake")
add_subdirectory("${ADVKBD_SOURCE_ROOT}/tools/RadixModel" converter)
