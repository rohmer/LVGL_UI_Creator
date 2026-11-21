#include "PNGToArray.h"

std::vector<unsigned char> PNGToArray(std::string fileName)
{
    unsigned int w, h;
    lodepng::State state;
    std::vector<unsigned char> img;
    unsigned error = lodepng::decode(img, w, h, fileName);
    return img;
}
