#include "../../../3rdParty/Catch2/single_include/catch2/catch.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "../../../ImgLib/LVImageConvert.h"

TEST_CASE("JPEG To RGBA 1")
{
    std::vector<unsigned char> img = LVImageConvert::ToRGBA("jpeg1.jpg");
    REQUIRE(img.size() == 3888 * 2592 * 4);    
}

TEST_CASE("JPEG To RGBA 2")
{
    std::vector<unsigned char> img = LVImageConvert::ToRGBA("jpeg2.jpg");
    REQUIRE(img.size() == 1600 * 1200 * 4);
}

TEST_CASE("JPEG To RGBA 3")
{
    std::vector<unsigned char> img = LVImageConvert::ToRGBA("jpeg3.jpg");
    REQUIRE(img.size() == 1200 * 800 * 4);
}

TEST_CASE("JPEG To RGBA 4")
{
    std::vector<unsigned char> img = LVImageConvert::ToRGBA("jpeg4.jpg");
    REQUIRE(img.size() == 5898 * 3831 * 4);
}

TEST_CASE("JPEG To RGB 1")
{
    std::vector<unsigned char> img = LVImageConvert::ToRGB("jpeg1.jpg");
    REQUIRE(img.size() == 3888 * 2592 * 3);
}

TEST_CASE("JPEG To RGB 2")
{
    std::vector<unsigned char> img = LVImageConvert::ToRGB("jpeg2.jpg");
    REQUIRE(img.size() == 1600 * 1200 * 3);
}

TEST_CASE("JPEG To RGB 3")
{
    std::vector<unsigned char> img = LVImageConvert::ToRGB("jpeg3.jpg");
    REQUIRE(img.size() == 1200 * 800 * 3);
}

TEST_CASE("JPEG To RGB 4")
{
    std::vector<unsigned char> img = LVImageConvert::ToRGB("jpeg4.jpg");
    REQUIRE(img.size() == 5898 * 3831 * 3);
}
