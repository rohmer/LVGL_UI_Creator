#include "Target.h"
#include <Serialization/Base64.h>
#include <Serialization/ImageRecords.h>
#include <cstring>
#include <stdexcept>
namespace CGeneration
{
void NormalizePixels(nlohmann::json &record, bool canvas)
{
    using json = nlohmann::json;
    auto width = record.value(canvas ? "width" : "w", record.value("header.w", 0u));
    auto height = record.value(canvas ? "height" : "h", record.value("header.h", 0u));
    if (!width || !height || width > 65535 || height > 65535)
        throw std::invalid_argument("Invalid image/canvas size for target conversion");
    auto format = canvas ? static_cast<lv_color_format_t>(
                               record.value("format", record.value("cf", int(LV_COLOR_FORMAT_ARGB8888))))
                         : Serialization::ReadImageFormat(record);
    // Wire color-format numbers are stable even when the native release lacks a format.
    unsigned bpp=0;
    switch(int(format)) {case 7:case 11:bpp=1;break;case 8:case 12:bpp=2;break;case 9:case 13:bpp=4;break;
    case 6:case 10:case 14:bpp=8;break;case 18:case 20:case 21:case 27:bpp=16;break;case 15:case 19:bpp=24;break;case 16:case 17:bpp=32;break;}
    if (!bpp)
        throw std::invalid_argument("Unsupported pixel format for target conversion");
    auto stride = record.value("stride", uint32_t((uint64_t(width) * bpp + 7) / 8));
    if (stride < (uint64_t(width) * bpp + 7) / 8)
        throw std::invalid_argument("Pixel stride is too small");
    uint64_t palette = LV_COLOR_INDEXED_PALETTE_SIZE(format) * 4;
    uint64_t expected = uint64_t(stride) * height + palette;
    if (format == LV_COLOR_FORMAT_RGB565A8)
        expected += uint64_t(stride / 2) * height;
    if (expected > UINT32_MAX || uint64_t(width) * height > 64u * 1024u * 1024u)
        throw std::invalid_argument("Image/canvas conversion exceeds supported size");
    std::string input =
        record.contains("data") ? base64_decode(record.at("data")) : std::string((size_t)expected, 0);
    if (input.size() != expected)
        throw std::invalid_argument("Image/canvas pixel payload has the wrong size");
    std::string pixels(size_t(width) * height * 4, 0);
    auto *bytes = (const uint8_t *)input.data();
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
        {
            auto *output = (uint8_t *)pixels.data() + (size_t(y) * width + x) * 4;
            auto *row = bytes + palette + size_t(y) * stride;
            output[3] = 255;
            if (format == LV_COLOR_FORMAT_ARGB8888 || format == LV_COLOR_FORMAT_XRGB8888)
            {
                memcpy(output, row + x * 4, 3);
                if (format == LV_COLOR_FORMAT_ARGB8888)
                    output[3] = row[x * 4 + 3];
            }
            else if (format == LV_COLOR_FORMAT_RGB888)
                memcpy(output, row + x * 3, 3);
            else if (format == LV_COLOR_FORMAT_RGB565 || format == LV_COLOR_FORMAT_RGB565A8 ||
                     format == LV_COLOR_FORMAT_RGB565_SWAPPED || format == LV_COLOR_FORMAT_ARGB8565)
            {
                size_t index = x * (format == LV_COLOR_FORMAT_ARGB8565 ? 3 : 2);
                uint16_t color = format == LV_COLOR_FORMAT_RGB565_SWAPPED
                                     ? (row[index] << 8) | row[index + 1]
                                     : row[index] | (row[index + 1] << 8);
                output[0] = uint8_t((color & 31) * 255 / 31);
                output[1] = uint8_t(((color >> 5) & 63) * 255 / 63);
                output[2] = uint8_t((color >> 11) * 255 / 31);
                if (format == LV_COLOR_FORMAT_RGB565A8)
                    output[3] = bytes[size_t(stride) * height + size_t(y) * (stride / 2) + x];
                else if (format == LV_COLOR_FORMAT_ARGB8565)
                    output[3] = row[index + 2];
            }
            else if (format == LV_COLOR_FORMAT_L8 || format == LV_COLOR_FORMAT_AL88)
            {
                uint8_t luminance = row[x * (format == LV_COLOR_FORMAT_AL88 ? 2 : 1)];
                output[0] = output[1] = output[2] = luminance;
                if (format == LV_COLOR_FORMAT_AL88)
                    output[3] = row[x * 2 + 1];
            }
            else if (LV_COLOR_FORMAT_IS_INDEXED(format))
            {
                unsigned index = (row[(x * bpp) / 8] >> (8 - bpp - (x * bpp) % 8)) & ((1u << bpp) - 1);
                memcpy(output, bytes + index * 4, 4);
            }
            else if (format == LV_COLOR_FORMAT_A1 || format == LV_COLOR_FORMAT_A2 ||
                     format == LV_COLOR_FORMAT_A4 || format == LV_COLOR_FORMAT_A8)
            {
                unsigned alpha = (row[(x * bpp) / 8] >> (8 - bpp - (x * bpp) % 8)) & ((1u << bpp) - 1);
                output[0] = output[1] = output[2] = 255;
                output[3] = uint8_t(alpha * 255 / ((1u << bpp) - 1));
            }
            else
                throw std::invalid_argument("Pixel format cannot be converted for this target");
        }
    if (canvas && !record.contains("data") && record.contains("fillColor"))
    {
        uint32_t color = record.at("fillColor");
        auto opacity = record.value("fillOpacity", 255);
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            pixels[i] = char(color);
            pixels[i + 1] = char(color >> 8);
            pixels[i + 2] = char(color >> 16);
            pixels[i + 3] = char(opacity);
        }
    }
    record["data"] = base64_encode((const unsigned char *)pixels.data(), pixels.size());
    record["stride"] = width * 4;
    record["formatVersion"] = 9;
    record["cf"] = LV_COLOR_FORMAT_ARGB8888;
    if (canvas)
    {
        record["format"] = LV_COLOR_FORMAT_ARGB8888;
        record["width"] = width;
        record["height"] = height;
    }
    else
    {
        record["w"] = width;
        record["h"] = height;
    }
}
} // namespace CGeneration
