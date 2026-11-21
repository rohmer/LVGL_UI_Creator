#pragma once

#include "Base64.h"
#include "WidgetRecords.h"
#include "ImageAsset.h"
#include "AssetProvider.h"

#if LVGL_VERSION_MAJOR == 8 && defined(LV_RUNTIME_PORTABLE)
#include "ImageRecords8.h"
#elif LVGL_VERSION_MAJOR >= 9
#include <cstring>

namespace Serialization
{
    struct SerializedBuffer
    {
        lv_draw_buf_t* buffer = nullptr;
        ~SerializedBuffer()
        {
            if (buffer)
                lv_draw_buf_destroy(buffer);
        }
    };

    inline lv_color_format_t ReadImageFormat(const json& record)
    {
        const auto format = record.value("cf", 0);
        if (record.value("formatVersion", 6) == 9)
            return static_cast<lv_color_format_t>(format);
        switch (format)
        {
        case 4: return LV_COLOR_FORMAT_XRGB8888;
        case 5: case 6: return LV_COLOR_FORMAT_ARGB8888;
        case 7: return LV_COLOR_FORMAT_I1;
        case 8: return LV_COLOR_FORMAT_I2;
        case 9: return LV_COLOR_FORMAT_I4;
        case 10: return LV_COLOR_FORMAT_I8;
        case 11: return LV_COLOR_FORMAT_A1;
        case 12: return LV_COLOR_FORMAT_A2;
        case 13: return LV_COLOR_FORMAT_A4;
        case 14: return LV_COLOR_FORMAT_A8;
        default: return LV_COLOR_FORMAT_UNKNOWN;
        }
    }

    inline lv_draw_buf_t* RestorePixels(const json& record, const char* widthKey, const char* heightKey)
    {
        const auto format = ReadImageFormat(record);
        const auto width = record.value(widthKey, 0u);
        const auto height = record.value(heightKey, 0u);
        if (format == LV_COLOR_FORMAT_UNKNOWN || width == 0 || height == 0 ||
            width > UINT16_MAX || height > UINT16_MAX || !record.contains("data") || !record["data"].is_string())
            return nullptr;
        auto data = base64_decode(record["data"].get<std::string>());
        const uint32_t stride = record.value("stride", lv_draw_buf_width_to_stride(width, format));
        const uint32_t palette = LV_COLOR_INDEXED_PALETTE_SIZE(format) * sizeof(lv_color32_t);
        const auto expectedSize = static_cast<uint64_t>(stride) * height + palette;
        if (stride < (static_cast<uint64_t>(width) * lv_color_format_get_bpp(format) + 7) / 8 ||
            expectedSize != data.size() || expectedSize > UINT32_MAX)
            return nullptr;
        auto* buffer = lv_draw_buf_create(width, height, format, stride);
        if (!buffer)
            return nullptr;
        std::memcpy(buffer->data, data.data(), data.size());
        if (record.value("formatVersion", 6) == 6 && record.value("cf", 0) == 6)
        {
            for (uint32_t row = 0; row < height; ++row)
                for (uint32_t column = 0; column < width; ++column)
                {
                    auto* pixel = buffer->data + row * stride + column * 4;
                    pixel[3] = pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 ? 0 : 255;
                }
        }
        return buffer;
    }

    inline void SavePixels(json& record, const lv_image_dsc_t& image, const char* widthKey, const char* heightKey)
    {
        record["formatVersion"] = 9;
        record["cf"] = static_cast<uint32_t>(image.header.cf);
        record["stride"] = static_cast<uint32_t>(image.header.stride);
        record[widthKey] = static_cast<uint32_t>(image.header.w);
        record[heightKey] = static_cast<uint32_t>(image.header.h);
        const uint32_t palette = LV_COLOR_INDEXED_PALETTE_SIZE(image.header.cf) * sizeof(lv_color32_t);
        const auto bytes = static_cast<uint32_t>(image.header.stride) * image.header.h + palette;
        if (image.data && image.data_size >= bytes)
            record["data"] = base64_encode(image.data, bytes);
    }

    struct SerializedImageSource
    {
        std::string text;
        json assetRecord;
        const void* providedSource=nullptr;
        SerializedBuffer pixels;
        std::vector<uint8_t> encoded;
        lv_image_dsc_t descriptor{};
        std::vector<lv_draw_buf_t*> previousPixels;
        ~SerializedImageSource()
        {
            for (auto* buffer : previousPixels) lv_draw_buf_destroy(buffer);
        }
    };

    inline json SaveImageSourceRecord(const void* source)
    {
        if (!source) return nullptr;
        if(currentAssetProvider){auto record=currentAssetProvider->Save(source);if(!record.is_null())return record;}
        json record;
        const auto type = lv_image_src_get_type(source);
        if (type == LV_IMAGE_SRC_FILE || type == LV_IMAGE_SRC_SYMBOL)
            return {{"kind", "text"}, {"source", static_cast<const char*>(source)}};
        if (type != LV_IMAGE_SRC_VARIABLE) return nullptr;
        const auto& image = *static_cast<const lv_image_dsc_t*>(source);
        SavePixels(record, image, "w", "h");
        const bool encoded = image.header.cf == LV_COLOR_FORMAT_RAW || image.header.cf == LV_COLOR_FORMAT_RAW_ALPHA;
        record["kind"] = encoded ? "encoded" : "pixels";
        if (encoded && image.data)
            record["data"] = base64_encode(image.data, image.data_size);
        return record;
    }

    inline const void* RestoreImageSourceRecord(const json& record, SerializedImageSource& source)
    {
        source.assetRecord=nullptr;source.providedSource=nullptr;
        if (!record.is_object()) return nullptr;
        if(record.contains("asset")) {
            if(currentAssetProvider){source.assetRecord=record;source.providedSource=currentAssetProvider->Resolve(record.at("asset"));return source.providedSource;}
            auto decoded=ImageAsset::Decode(record.at("asset"));source.encoded.assign(decoded.pixels.begin(),decoded.pixels.begin()+size_t(decoded.width)*decoded.height*4);
            source.descriptor={};source.descriptor.header.magic=LV_IMAGE_HEADER_MAGIC;source.descriptor.header.cf=LV_COLOR_FORMAT_ARGB8888;
            source.descriptor.header.w=decoded.width;source.descriptor.header.h=decoded.height;source.descriptor.header.stride=decoded.width*4;
            source.descriptor.data=source.encoded.data();source.descriptor.data_size=decoded.width*decoded.height*4;source.assetRecord=record;
            return &source.descriptor;
        }
        const auto kind = record.value("kind", std::string{});
        if (kind == "text")
        {
            source.text = record.value("source", std::string{});
            return source.text.c_str();
        }
        if (kind == "encoded")
        {
            const auto bytes = base64_decode(record.value("data", std::string{}));
            source.encoded.assign(bytes.begin(), bytes.end());
            source.descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
            source.descriptor.header.cf = record.value("cf", int(LV_COLOR_FORMAT_RAW));
            source.descriptor.header.w = record.value("w", 0u);
            source.descriptor.header.h = record.value("h", 0u);
            source.descriptor.header.stride = record.value("stride", 0u);
            source.descriptor.data = source.encoded.data();
            source.descriptor.data_size = static_cast<uint32_t>(source.encoded.size());
            return &source.descriptor;
        }
        if (source.pixels.buffer) source.previousPixels.push_back(source.pixels.buffer);
        source.pixels.buffer = RestorePixels(record, "w", "h");
        return source.pixels.buffer;
    }
}
#endif
