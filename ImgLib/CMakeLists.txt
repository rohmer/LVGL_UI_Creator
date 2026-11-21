add_library(ImgLib STATIC LVImageConvert.cpp)
target_include_directories(ImgLib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ImgLib PUBLIC imagequant)
