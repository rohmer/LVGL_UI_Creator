#pragma once
#include "stb_image.h"
#include "../3rdParty/libimagequant/libimagequant.h"
#include <string>
#include <vector>

struct sColor
{
    unsigned char r, g, b, a;
};
struct sPaletteImageResult
{
    std::vector<unsigned char> imageData;
    std::vector<sColor> palette;
};

enum ePaletteBits
{
    One = 1,
    Two = 2,
    Four = 4,
    Eight = 8
};

class LVImageConvert
{    
public:
    static std::vector<unsigned char> ToRGBA(std::string file);
    static std::vector<unsigned char> ToRGB(std::string file);
    static sPaletteImageResult ToPalette(std::string file, ePaletteBits palletteSize, float ditheringLevel = 1.0f);
};
