#include "ImageAsset.h"
#include "RuntimeFeatures.h"
#include "AssetProvider.h"
#include "WidgetRecords.h"
#include "Base64.h"
#ifndef LVSERIAL_EMBEDDED_RUNTIME
#include <fstream>
#endif
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#if __has_include(<src/misc/cache/instance/lv_image_cache.h>)
#include <src/misc/cache/instance/lv_image_cache.h>
#include <src/misc/cache/instance/lv_image_header_cache.h>
#elif LVGL_VERSION_MAJOR == 9
#include <src/misc/cache/lv_image_cache.h>
#endif
#if LVSERIAL_ENABLE_LEGACY_IMAGE_DECODERS
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#if !defined(LVSERIAL_ENABLE_LEGACY_PNG) || LVSERIAL_ENABLE_LEGACY_PNG
#define STBI_ONLY_PNG
#endif
#if !defined(LVSERIAL_ENABLE_LEGACY_JPEG) || LVSERIAL_ENABLE_LEGACY_JPEG
#define STBI_ONLY_JPEG
#endif
#if !defined(LVSERIAL_ENABLE_LEGACY_GIF) || LVSERIAL_ENABLE_LEGACY_GIF
#define STBI_ONLY_GIF
#endif
#define STBI_NO_STDIO
#include "../../ImgLib/stb_image.h"
#endif

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
namespace Serialization
{
    namespace
    {
        struct AssetData
        {
            std::vector<uint8_t> pixels;
            std::vector<lv_image_dsc_t> frames;
            std::vector<int> delays;
            lv_timer_t* timer = nullptr;
            lv_obj_t* image = nullptr;
            size_t frame = 0;
            void Clear()
            {
                if (timer) { lv_timer_delete(timer); timer = nullptr; }
                for (const auto& descriptor : frames) { lv_image_cache_drop(&descriptor); lv_image_header_cache_drop(&descriptor); }
                frames.clear(); pixels.clear(); delays.clear(); frame = 0;
            }
            ~AssetData() { Clear(); }
        };
        std::string Extension(std::string name)
        {
            const auto dot = name.rfind('.');
            if (dot == std::string::npos) return {};
            auto extension = name.substr(dot + 1);
            for (auto& ch : extension) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return extension;
        }
    }
    DecodedImage ImageAsset::Decode(const json& asset)
    {
#if LVSERIAL_ENABLE_LEGACY_IMAGE_DECODERS
        if(asset.contains("previewFrames")) {
            if(!asset["previewFrames"].is_array()||asset["previewFrames"].empty())throw std::invalid_argument("Image has no preview frames");
            DecodedImage result;size_t index=0;
            for(const auto& frame:asset["previewFrames"]) {
                auto decoded=Decode({{"name","preview.png"},{"sourceData",frame}});
                if(index&&(decoded.width!=result.width||decoded.height!=result.height))throw std::invalid_argument("Image frames have different dimensions");
                result.width=decoded.width;result.height=decoded.height;result.pixels.insert(result.pixels.end(),decoded.pixels.begin(),decoded.pixels.end());
                result.delays.push_back(asset.contains("delays")&&index<asset["delays"].size()?std::max(20u,asset["delays"][index].get<uint32_t>()):100u);++index;
            }
            return result;
        }
        const auto format = Extension(asset.value("name", std::string{}));
        if (format != "gif" && format != "png" && format != "jpg" && format != "jpeg") throw std::invalid_argument("Choose a GIF, PNG, or JPG image");
        const auto encoded = base64_decode(asset.at("sourceData").get<std::string>());
        if (encoded.empty() || encoded.size() > INT_MAX) throw std::invalid_argument("Image file is empty or too large");
        int width = 0, height = 0, channels = 0, count = 1;
        if (!stbi_info_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()), static_cast<int>(encoded.size()), &width, &height, &channels) ||
            width < 1 || height < 1 || width > 65535 || height > 65535 || uint64_t(width) * height > 16 * 1024 * 1024)
            throw std::invalid_argument("Invalid image or image dimensions are too large");
        int* frameDelays = nullptr;
        stbi_uc* decoded = nullptr;
#if !defined(LVSERIAL_ENABLE_LEGACY_GIF) || LVSERIAL_ENABLE_LEGACY_GIF
        if(format == "gif") decoded=stbi_load_gif_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()), static_cast<int>(encoded.size()), &frameDelays,
            &width, &height, &count, &channels, 4);
        else
#endif
            decoded=stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()), static_cast<int>(encoded.size()), &width, &height, &channels, 4);
        std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(decoded, stbi_image_free);
        std::unique_ptr<int, decltype(&stbi_image_free)> delays(frameDelays, stbi_image_free);
        if (!decoded || count < 1) throw std::invalid_argument(std::string("Could not decode the image: ")+(stbi_failure_reason()?stbi_failure_reason():"invalid frame count"));
        const auto frameSize = size_t(width) * height * 4;
        if (frameSize > SIZE_MAX / static_cast<size_t>(count)) throw std::invalid_argument("Image data is too large");
        std::vector<uint8_t> owned(decoded, decoded + frameSize * count);
        for (size_t i = 0; i < owned.size(); i += 4) std::swap(owned[i], owned[i + 2]);
        std::vector<uint32_t> timings(count, 100);
        for (int i = 0; frameDelays && i < count; ++i) timings[i] = std::max(20, frameDelays[i]);
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height), std::move(owned), std::move(timings)};
#else
        throw std::invalid_argument("Encoded legacy image decoding is disabled; export device frames with the project");
#endif
    }
    void ImageAsset::Restore(lv_obj_t* image, const json& asset)
    {

#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
        if(currentAssetProvider){lv_image_set_src(image,currentAssetProvider->Resolve(asset));auto record=ReadSection(image,"img");record["asset"]=asset;record["srcType"]=0;record.erase("data");record.erase("encodedSource");record.erase("fileName");record.erase("sym");PreserveSection(image,"img",record);return;}
        auto decoded = Decode(asset);
        const auto width = decoded.width, height = decoded.height;
        const auto count = decoded.delays.size();
        const auto frameSize = size_t(width) * height * 4;
        auto owned = std::move(decoded.pixels);
        std::vector<int> timings(decoded.delays.begin(), decoded.delays.end());
        auto& state = WidgetData<AssetData>(image);
        state.Clear(); state.image = image; state.pixels = std::move(owned); state.delays = std::move(timings);
        state.frames.resize(count);
        for (int i = 0; i < count; ++i)
        {
            auto& descriptor = state.frames[i];
            #if LVGL_VERSION_MAJOR >= 9
            descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
            #endif
            descriptor.header.cf =
#if LVGL_VERSION_MAJOR == 8
                LV_IMG_CF_TRUE_COLOR_ALPHA;
#else
                LV_COLOR_FORMAT_ARGB8888;
#endif
            descriptor.header.w = width; descriptor.header.h = height;
#if LVGL_VERSION_MAJOR >= 9
            descriptor.header.stride = width * 4;
#endif
            descriptor.data_size = static_cast<uint32_t>(frameSize);
            descriptor.data = state.pixels.data() + frameSize * i;
        }
        lv_image_set_src(image, &state.frames[0]);
        if (count > 1)
            state.timer = lv_timer_create([](lv_timer_t* timer) {
                auto* state = static_cast<AssetData*>(lv_timer_get_user_data(timer));
                state->frame = (state->frame + 1) % state->frames.size();
                lv_image_set_src(state->image, &state->frames[state->frame]);
                lv_timer_set_period(timer, state->delays[state->frame]);
            }, state.delays[0], &state);
        auto record = ReadSection(image, "img");
        record["asset"] = asset;
        record["asset"]["width"] = width; record["asset"]["height"] = height; record["asset"]["frames"] = count;
        record["srcType"] = 0;
        record.erase("fileName"); record.erase("sym"); record.erase("data"); record.erase("encodedSource");
        PreserveSection(image, "img", record);
#else
        throw std::invalid_argument("Image widget disabled in this runtime");
#endif
    }
#ifndef LVSERIAL_EMBEDDED_RUNTIME
    void ImageAsset::LoadFromFile(lv_obj_t* image, const std::filesystem::path& path)
    {
        const auto format = Extension(path.filename().u8string());
        if (format != "gif" && format != "png" && format != "jpg" && format != "jpeg") throw std::invalid_argument("Choose a GIF, PNG, or JPG image");
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Could not open the image file");
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        Restore(image, {{"name", path.filename().u8string()}, {"sourceData", base64_encode(bytes.data(), bytes.size())}});
        const auto* descriptor = static_cast<const lv_image_dsc_t*>(lv_image_get_src(image));
        auto* parent = lv_obj_get_parent(image);
        lv_obj_update_layout(parent);
        lv_obj_set_size(image, std::min(int(descriptor->header.w), std::max(1, int(lv_obj_get_content_width(parent)))),
            std::min(int(descriptor->header.h), std::max(1, int(lv_obj_get_content_height(parent)))));
        lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
    }
#endif
}
#endif
