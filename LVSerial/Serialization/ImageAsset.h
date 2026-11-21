#pragma once
#include "LVObject.h"
#ifndef LVSERIAL_EMBEDDED_RUNTIME
#include <filesystem>
#endif
#include <vector>
namespace Serialization
{
    struct DecodedImage
    {
        uint32_t width = 0, height = 0;
        std::vector<uint8_t> pixels;
        std::vector<uint32_t> delays;
    };
    class ImageAsset
    {
    public:
        static DecodedImage Decode(const json& asset);
#ifndef LVSERIAL_EMBEDDED_RUNTIME
        static void LoadFromFile(lv_obj_t* image, const std::filesystem::path& path);
#endif
        static void Restore(lv_obj_t* image, const json& asset);
    };
}
