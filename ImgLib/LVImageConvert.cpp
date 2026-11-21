#include "LVImageConvert.h"

std::vector<unsigned char> LVImageConvert::ToRGBA(std::string file)
{
    int w, h, n, comp;
    int res = stbi_info(file.c_str(), &w, &h, &comp);
    if (!res)
        return std::vector<unsigned char>();
    unsigned char* data = stbi_load(file.c_str(), &w, &h, &n, 4);
    std::vector<unsigned char> ret(data,data+w*h*4);

    return ret;
}

std::vector<unsigned char> LVImageConvert::ToRGB(std::string file)
{
    int w, h, n, comp;
    int res = stbi_info(file.c_str(), &w, &h, &comp);
    if (!res)
        return std::vector<unsigned char>();
    unsigned char* data = stbi_load(file.c_str(), &w, &h, &n, 3);
    std::vector<unsigned char> ret(data, data + w * h * 3);

    return ret;
}

sPaletteImageResult LVImageConvert::ToPalette(std::string file, ePaletteBits palletteSize, float ditheringLevel)
{
    int w, h, n, comp;
    int res = stbi_info(file.c_str(), &w, &h, &comp);
    if (!res)
        return sPaletteImageResult();
    unsigned char* data = stbi_load(file.c_str(), &w, &h, &n, 3);
    liq_attr* handle = liq_attr_create();
    liq_image* input_image = liq_image_create_rgba(handle, data, w, h, 0);

}