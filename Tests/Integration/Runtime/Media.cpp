#include "FontConverter.h"
#include "ImageConverter.h"
#include "lvgl_runtime.h"
#include <Serialization/LVFont.h>
#include <Serialization/Protobuf.h>
#include <Serialization/Xml.h>
#include <Serialization/ResourceArena.h>
#include <Serialization/FontData.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <src/image/lv_image_decoder_private.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
            throw std::runtime_error(std::string("Line ") + std::to_string(__LINE__) + ": " #x);             \
    } while (0)
using json = nlohmann::json;
using Handle = std::unique_ptr<lvgl_runtime_t, decltype(&lvgl_runtime_destroy)>;
Handle Load(const json &project,bool xml=false)
{
    auto data = Serialization::Protobuf::Encode(Serialization::EntityKind::Project, "project", project);
    if(xml){auto text=Serialization::Xml::Encode(Serialization::EntityKind::Project,"project",project);data.assign(text.begin(),text.end());}
    lvgl_runtime_t *runtime = nullptr;
    char error[512];
    if (!lvgl_runtime_load_memory(data.data(), data.size(), nullptr, &runtime, error, sizeof error))
        throw std::runtime_error(error);
    return Handle(runtime, lvgl_runtime_destroy);
}
std::vector<uint8_t> GlyphPixels(const lv_font_t* font,lv_font_glyph_dsc_t& glyph) {
    CHECK(lv_font_get_glyph_dsc(font,&glyph,'A','V'));
    auto* bitmap=lv_draw_buf_create(glyph.box_w,glyph.box_h,LV_COLOR_FORMAT_A8,LV_STRIDE_AUTO);
    CHECK(bitmap);CHECK(lv_font_get_glyph_bitmap(&glyph,bitmap));
    std::vector<uint8_t> pixels;
    for(unsigned y=0;y<glyph.box_h;++y) {
        auto* row=bitmap->data+y*bitmap->header.stride;
        pixels.insert(pixels.end(),row,row+glyph.box_w);
    }
    lv_font_glyph_release_draw_data(&glyph);lv_draw_buf_destroy(bitmap);
    return pixels;
}
void SaveCase(const std::filesystem::path& path, const json& project) {
    auto data=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",project);
    std::ofstream out(path,std::ios::binary);out.write((const char*)data.data(),data.size());
    if(!out)throw std::runtime_error("Could not save media test fixture");
}
int main(int argc, char **argv)
{
    try
    {
        CHECK(argc == 3);
        auto root = std::filesystem::path(argv[1]), output = std::filesystem::path(argv[2]);
        std::filesystem::create_directories(output);
        lv_init();
        auto *display = lv_display_create(320, 240);
        auto *previous = lv_screen_active();
        size_t images = 0, fonts = 0;
        const char *formats[] = {"ARGB8888",
                                 "XRGB8888",
                                 "RGB888",
                                 "RGB565",
                                 "RGB565_SWAPPED",
                                 "RGB565A8",
                                 "ARGB8565",
                                 "ARGB8888_PREMULTIPLIED",
                                 "L8",
                                 "AL88",
                                 "A1",
                                 "A2",
                                 "A4",
                                 "A8",
                                 "I1",
                                 "I2",
                                 "I4",
                                 "I8",
                                 "AUTO",
                                 "RAW",
                                 "RAW_ALPHA"};
        for (const char *format : formats)
            for (const char *compression : {"NONE", "RLE", "LZ4"})
            {
                if ((std::string(format) == "RAW" || std::string(format) == "RAW_ALPHA") &&
                    std::string(compression) != "NONE")
                    continue;
                std::cerr << "Runtime image " << format << ' ' << compression << '\n';
                auto result = ImageConversion::Convert(
                    {root / "Tests/Fixtures/Images/sample.png"},
                    {{"colorFormat", format},
                     {"compression", compression},
                     {"align",
                      (std::string(format) == "RAW" || std::string(format) == "RAW_ALPHA") ? 1 : 4}});
                if (!result.success)
                    throw std::runtime_error(result.message);
                json project = {
                    {"obj",
                     {{{"base", json::object()},
                       {"img", {{"asset", {{"projectImage", "image_1"}}}}},
                       {"oid", 0}}}},
                    {"images", {{{"id", "image_1"}, {"name", "Image"}, {"image", result.assets[0]}}}}};
                SaveCase(output / ("image_"+std::string(format)+"_"+compression+".pb"),project);
                {
                    auto runtime = Load(project);
                    auto *image =
                        static_cast<const lv_image_dsc_t *>(lvgl_runtime_image(runtime.get(), "image_1"));
                    CHECK(image);
                    CHECK(lv_image_get_src(lvgl_runtime_object(runtime.get(), 0, 0)) == image);
                    CHECK(image->header.cf == result.assets[0]["deviceFrames"][0]["cf"].get<unsigned>());
                    lv_image_decoder_dsc_t decoded{};
                    auto status = lv_image_decoder_open(&decoded, image, nullptr);
                    if (status != LV_RESULT_OK)
                        throw std::runtime_error(std::string("Native decoder rejected ") + format + " " +
                                                 compression);
                    CHECK(decoded.decoded);
                    lv_image_decoder_close(&decoded);
                    ++images;
                }
                CHECK(lv_screen_active() == previous);
            }
        for (int bpp : {1, 2, 3, 4, 8})
            for (bool compress : {false, true})
            {
                FontConversion::Options options;
                options.sources.push_back(
                    {root / "3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf", "0x20-0x5A"});
                options.bpp = bpp;
                options.compress = compress;
                options.name = "runtime_font";
                options.output =
                    output / ("font_" + std::to_string(bpp) + "_" + std::to_string(compress) + ".c");
                if (bpp == 3 && !compress)
                {
                    // LVGL has no raw 3-bpp decoder. Keep ten meaningful
                    // fixtures by using plain RLE for this variant; the other
                    // 3-bpp case exercises RLE with XOR prefiltering.
                    auto invalid = options;
                    invalid.format = "bin";
                    invalid.output = output / "unsupported_raw_3.bin";
                    CHECK(!FontConversion::Convert(invalid).success);
                    options.compress = true;
                    options.prefilter = false;
                }
                auto converted = FontConversion::Prepare(options);
                if (!converted.success)
                    throw std::runtime_error(converted.message);
                auto font=converted.font;
                CHECK(Serialization::FontData::Matches(font));
                Serialization::ResourceArena arena;
                std::vector<uint8_t> expectedPixels;
                lv_font_glyph_dsc_t expectedGlyph{};
                {
                    Serialization::ResourceScope scope(arena);
                    auto& source=Serialization::LVFont::FromJSON(font);
                    expectedPixels=GlyphPixels(&source,expectedGlyph);
                    CHECK(Serialization::LVFont::ToJSON(source)==font);
                }
                json project = {
                    {"obj",
                     {{{"base",
                        {{"fontId","font_1"}}},
                       {"text", "AV 123"},
                       {"oid", 0}}}},
                    {"fonts", {{{"id", "font_1"}, {"name", "Body"}, {"font", font}}}}};
                SaveCase(output / ("font_"+std::to_string(bpp)+"_"+std::to_string(compress)+".pb"),project);
                for(bool xml:{false,true}) {
                    auto runtime = Load(project,xml);
                    auto *loaded = lvgl_runtime_font(runtime.get(), "font_1");
                    CHECK(loaded);
                    CHECK(lv_obj_get_style_text_font(lvgl_runtime_object(runtime.get(), 0, 0),
                                                     LV_PART_MAIN) == loaded);
                    lv_font_glyph_dsc_t glyph{};
                    CHECK(GlyphPixels(loaded,glyph)==expectedPixels);
                    CHECK(glyph.adv_w==expectedGlyph.adv_w);CHECK(glyph.box_w==expectedGlyph.box_w);
                    CHECK(glyph.box_h==expectedGlyph.box_h);CHECK(glyph.ofs_x==expectedGlyph.ofs_x);CHECK(glyph.ofs_y==expectedGlyph.ofs_y);
                    CHECK(loaded->line_height==font["lineHeight"].get<int>());
                }
                ++fonts;
                CHECK(lv_screen_active() == previous);
            }
        lv_display_delete(display);
        lv_deinit();
        std::cout << images << " image format/compression combinations and " << fonts
                  << " font combinations passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
