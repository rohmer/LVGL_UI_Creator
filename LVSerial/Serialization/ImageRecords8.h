#pragma once
#include <cstring>
#include <stdexcept>
namespace Serialization {
struct SerializedBuffer {
    lv_draw_buf_t* buffer = nullptr;
    ~SerializedBuffer() { if(buffer) { lv_mem_free((void*)buffer->image.data); delete buffer; } }
};
inline int ReadImageFormat(const json& record) { return record.value("cf", 16); }
inline lv_draw_buf_t* RestorePixels(const json& record, const char* wk, const char* hk) {
    unsigned w=record.value(wk,0u), h=record.value(hk,0u);
    if(!w || !h || w>2047 || h>2047 || !record.contains("data"))return nullptr;
    auto bytes=base64_decode(record.at("data").get<std::string>());
    if(bytes.size()!=size_t(w)*h*4)throw std::invalid_argument("Invalid normalized LVGL 8 image");
    auto* result=new lv_draw_buf_t{};
    auto* data=static_cast<uint8_t*>(lv_mem_alloc(bytes.size()));
    if(!data){delete result;throw std::bad_alloc();}
    std::memcpy(data,bytes.data(),bytes.size());
    lv_draw_buf_init(result,w,h,0,w*4,data,(uint32_t)bytes.size());return result;
}
inline void SavePixels(json& record, const lv_image_dsc_t& image, const char* wk, const char* hk) {
    record["formatVersion"]=9;record["cf"]=16;record[wk]=image.header.w;record[hk]=image.header.h;
    record["stride"]=unsigned(image.header.w)*4;
    if(image.data)record["data"]=base64_encode(image.data,image.data_size);
}
struct SerializedImageSource {
    std::string text;json assetRecord;const void* providedSource=nullptr;
    SerializedBuffer pixels;std::vector<uint8_t> encoded;lv_image_dsc_t descriptor{};
};
inline json SaveImageSourceRecord(const void* source) {
    if(!source)return nullptr;
    if(currentAssetProvider){auto r=currentAssetProvider->Save(source);if(!r.is_null())return r;}
    auto kind=lv_img_src_get_type(source);
    if(kind==LV_IMG_SRC_FILE || kind==LV_IMG_SRC_SYMBOL)return {{"kind","text"},{"source",(const char*)source}};
    json result;SavePixels(result,*(const lv_image_dsc_t*)source,"w","h");result["kind"]="pixels";return result;
}
inline const void* RestoreImageSourceRecord(const json& record, SerializedImageSource& source) {
    if(!record.is_object())return nullptr;
    if(record.contains("asset")) {
        if(!currentAssetProvider)throw std::invalid_argument("Missing runtime image provider");
        source.assetRecord=record;source.providedSource=currentAssetProvider->Resolve(record.at("asset"));return source.providedSource;
    }
    auto kind=record.value("kind",std::string{});
    if(kind=="text"){source.text=record.value("source",std::string{});return source.text.c_str();}
    if(kind=="encoded")throw std::invalid_argument("Import encoded images into the project image catalog for LVGL 8");
    if(source.pixels.buffer)throw std::invalid_argument("Replacing a live image buffer is unsupported");
    source.pixels.buffer=RestorePixels(record,"w","h");return source.pixels.buffer;
}
}
