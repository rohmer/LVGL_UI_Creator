#include "CCodeGenerator.h"
#include "ProjectAssets.h"
#include "Targets/Target.h"
#include "catch.hpp"
#include <Serialization/Base64.h>
#include <Serialization/Protobuf.h>
#include <filesystem>
#include <fstream>
#include <lvgl.h>
using json = nlohmann::json;
namespace
{
json Style(std::string key, json value)
{
    return {{"base", {{"nativeStyles", {{{"selector", 0}, {"values", {{key, value}}}}}}}}};
}
json Font(unsigned bpp = 4, unsigned stride = 4)
{
    std::string bytes("\x12\x30\x00\x00\x45\x60\x00\x00", 8);
    return {{"lineHeight", 8},
            {"dsc",
             {{"bpp", bpp},
              {"stride", stride},
              {"bitmapFmt", 0},
              {"bitmap", base64_encode((const unsigned char *)bytes.data(), bytes.size())},
              {"GlyphDsc", {{{"box_w", 3}, {"box_h", 2}, {"bitmapIdx", 0}}}}}}};
}
} // namespace
TEST_CASE("All requested target versions are explicit and unknown versions fail", "[targets][selection]")
{
    for (const char *version : {"9.6", "9.5", "9.3", "9.2", "9.1", "8.3"})
        CHECK_NOTHROW(CGeneration::ValidateTarget(version));
    for (const char *version : {"", "9", "9.4", "9.6.0", "v9.6", "8.4", "10.0", "9.6;bad"})
        CHECK_THROWS(CGeneration::ValidateTarget(version));
    auto input = Style("bg_color", 0x123456);
    input["font"] = Font();
    auto original = input;
    CGeneration::PrepareTarget(input, "9.6");
    CHECK(input == original);
}
TEST_CASE("Unsupported target features fail while inactive flags are removed", "[targets][validation]")
{
    for (const char *version : {"9.5", "9.3", "9.2", "9.1", "8.3"})
    {
        auto active = json{{"base", {{"flags", {{"not_a_real_flag", true}}}}}};
        CHECK_THROWS(CGeneration::PrepareTarget(active, version));
        auto inactive = json{{"base", {{"flags", {{"not_a_real_flag", false}, {"checked", true}}}}}};
        CHECK_NOTHROW(CGeneration::PrepareTarget(inactive, version));
        CHECK_FALSE(inactive["base"]["flags"].contains("not_a_real_flag"));
        CHECK(inactive["base"]["flags"]["checked"] == true);
        auto unknown = Style("not_a_real_style", 7);
        CHECK_THROWS(CGeneration::PrepareTarget(unknown, version));
        auto zero = Style("not_a_real_style", 0);
        CHECK_NOTHROW(CGeneration::PrepareTarget(zero, version));
        CHECK(zero["base"]["nativeStyles"][0]["values"].empty());
    }
    for (const char *version : {"9.1", "9.2", "8.3"})
    {
        auto input = json{{"slider", {{"orientation", 1}}}};
        CHECK_THROWS(CGeneration::PrepareTarget(input, version));
        input["slider"]["orientation"] = 0;
        CHECK_NOTHROW(CGeneration::PrepareTarget(input, version));
    }
    for (const char *version : {"9.1", "8.3"})
    {
        auto input = json{{"bar", {{"orientation", 2}}}};
        CHECK_THROWS(CGeneration::PrepareTarget(input, version));
    }
    {
        auto input = json{{"arc", {{"knobOffset", 7}}}};
        CHECK_THROWS(CGeneration::PrepareTarget(input, "8.3"));
    }
    {
        auto input = json{{"img", {{"scaleX", 256}, {"scaleY", 512}}}};
        CHECK_THROWS(CGeneration::PrepareTarget(input, "8.3"));
        input["img"]["scaleY"] = 256;
        input["img"]["innerAlign"] = 11;
        CHECK_THROWS(CGeneration::PrepareTarget(input, "8.3"));
    }
}
TEST_CASE("Portable dimensions and grid fractions preserve version semantics", "[targets][coordinates]")
{
    for (const char *version : {"9.1", "8.3"})
    {
        auto input =
            json{{"base",
                  {{"coords",
                    {{"width", LV_SIZE_CONTENT}, {"height", LV_PCT(75)}, {"x", LV_PCT(-25)}, {"y", 12}}}}}};
        CGeneration::PrepareTarget(input, version);
        auto coords = input["base"]["coords"];
        CHECK(coords["width"]["coordinateKind"] == "content");
        CHECK(coords["height"]["value"] == 75);
        CHECK(coords["x"]["value"] == -25);
        CHECK(coords["y"] == 12);
        auto tracks = Style("grid_column_dsc_array", json::array({40, LV_GRID_FR(2), LV_GRID_CONTENT}));
        CGeneration::PrepareTarget(tracks, version);
        auto values = tracks["base"]["nativeStyles"][0]["values"]["grid_column_dsc_array"];
        CHECK(values[0] == 40);
        CHECK(values[1]["coordinateKind"] == "gridFraction");
        CHECK(values[1]["value"] == 2);
        CHECK(values[2]["coordinateKind"] == "gridContent");
        auto invalid = json{{"base", {{"coords", {{"x", LV_PCT(1001)}}}}}};
        CHECK_THROWS(CGeneration::PrepareTarget(invalid, version));
    }
    {
        auto input = json{{"base", {{"coords", {{"x", 20000}}}}}};
        CHECK_THROWS(CGeneration::PrepareTarget(input, "8.3"));
    }
    for (const char *version : {"9.5", "9.3", "9.2"})
    {
        auto input = json{{"base", {{"coords", {{"height", LV_PCT(75)}}}}}};
        auto original = input;
        CGeneration::PrepareTarget(input, version);
        CHECK(input == original);
    }
}
TEST_CASE("Older font backends repack padded glyphs without changing their bits", "[targets][fonts]")
{
    for (const char *version : {"9.1", "9.2", "8.3"})
    {
        auto input = Font();
        CGeneration::PrepareTarget(input, version);
        auto decoded = base64_decode(input["dsc"]["bitmap"]);
        CHECK(decoded == std::string("\x12\x34\x56", 3));
        CHECK(input["dsc"]["stride"] == 0);
        CHECK(input["dsc"]["GlyphDsc"][0]["bitmapIdx"] == 0);
    }
    for (const char *version : {"9.5", "9.3"})
    {
        auto input = Font();
        auto original = input;
        CGeneration::PrepareTarget(input, version);
        CHECK(input == original);
    }
    for (unsigned bpp : {0u, 9u})
    {
        auto input = Font(bpp);
        CHECK_THROWS(CGeneration::PrepareTarget(input, "9.1"));
    }
    {
        auto input = Font();
        input["dsc"]["bitmap"] = "AA==";
        CHECK_THROWS(CGeneration::PrepareTarget(input, "8.3"));
    }
    {
        auto input = Font();
        input["dsc"]["bitmapFmt"] = 1;
        CHECK_THROWS(CGeneration::PrepareTarget(input, "9.2"));
    }
}
TEST_CASE("Older image exports retain originals and gradients fail explicitly", "[targets][assets]")
{
    for (const char *version : {"9.5", "9.3", "9.2", "9.1", "8.3"})
    {
        auto input = json{{"sourceData", "source"},
                          {"deviceFrames", json::array()},
                          {"conversion", {{"compression", "LZ4"}}}};
        CGeneration::PrepareTarget(input, version);
        CHECK_FALSE(input.contains("deviceFrames"));
        CHECK(input["sourceData"] == "source");
        CHECK(input["conversion"]["compression"] == "LZ4");
    }
    for (const char *version : {"9.1", "8.3"})
    {
        auto gradient = json{{"stops", {{{"opacity", 255}}}}, {"extend", 1}};
        CHECK_THROWS(CGeneration::PrepareTarget(gradient, version));
        gradient["extend"] = 0;
        CHECK_NOTHROW(CGeneration::PrepareTarget(gradient, version));
        gradient["points"] = {{0, 0}, {10, 10}};
        CHECK_THROWS(CGeneration::PrepareTarget(gradient, version));
    }
    {
        auto gradient = json{{"stops", {{{"opacity", 128}}}}};
        CHECK_THROWS(CGeneration::PrepareTarget(gradient, "8.3"));
    }
}
TEST_CASE("Failed generation leaves existing output untouched", "[targets][io]")
{
    auto path = std::filesystem::path(UICREATOR_UNIT_OUTPUT);
    std::filesystem::create_directories(path);
    auto destination = path / "ui.c";
    {
        std::ofstream file(destination);
        file << "keep this file";
    }
    CCodeGenerator generator;
    generator.SetTargetVersion("invalid");
    CHECK(generator.LoadProject(json{{"obj", json::array()}}));
    CHECK_FALSE(generator.ExportProject(path.u8string()));
    std::ifstream file(destination);
    std::string text((std::istreambuf_iterator<char>(file)), {});
    CHECK(text == "keep this file");
    CHECK_FALSE(generator.GetLastError().empty());
}

TEST_CASE("Pixel normalization preserves colors, alpha and channel layouts", "[targets][pixels]")
{
    struct Sample
    {
        lv_color_format_t format;
        std::string bytes;
        std::string expected;
    };
    std::vector<Sample> samples = {
        {LV_COLOR_FORMAT_ARGB8888, std::string("\x01\x02\x03\x04", 4), std::string("\x01\x02\x03\x04", 4)},
        {LV_COLOR_FORMAT_XRGB8888, std::string("\x01\x02\x03\x00", 4), std::string("\x01\x02\x03\xff", 4)},
        {LV_COLOR_FORMAT_RGB888, std::string("\x01\x02\x03", 3), std::string("\x01\x02\x03\xff", 4)},
        {LV_COLOR_FORMAT_RGB565, std::string("\x00\xf8", 2), std::string("\x00\x00\xff\xff", 4)},
        {LV_COLOR_FORMAT_RGB565_SWAPPED, std::string("\xf8\x00", 2), std::string("\x00\x00\xff\xff", 4)},
        {LV_COLOR_FORMAT_RGB565A8, std::string("\x00\xf8\x80", 3), std::string("\x00\x00\xff\x80", 4)},
        {LV_COLOR_FORMAT_ARGB8565, std::string("\x00\xf8\x40", 3), std::string("\x00\x00\xff\x40", 4)},
        {LV_COLOR_FORMAT_L8, std::string("\x80", 1), std::string("\x80\x80\x80\xff", 4)},
        {LV_COLOR_FORMAT_AL88, std::string("\x40\x80", 2), std::string("\x40\x40\x40\x80", 4)},
        {LV_COLOR_FORMAT_A1, std::string("\x80", 1), std::string("\xff\xff\xff\xff", 4)},
        {LV_COLOR_FORMAT_A2, std::string("\x80", 1), std::string("\xff\xff\xff\xaa", 4)},
        {LV_COLOR_FORMAT_A4, std::string("\x80", 1), std::string("\xff\xff\xff\x88", 4)},
        {LV_COLOR_FORMAT_A8, std::string("\x80", 1), std::string("\xff\xff\xff\x80", 4)}};
    for (auto format : {LV_COLOR_FORMAT_I1, LV_COLOR_FORMAT_I2, LV_COLOR_FORMAT_I4, LV_COLOR_FORMAT_I8})
    {
        auto bpp = lv_color_format_get_bpp(format);
        std::string bytes;
        for (unsigned i = 0; i < (1u << bpp); ++i)
        {
            bytes += char(i);
            bytes += char(i + 1);
            bytes += char(i + 2);
            bytes += char(i + 128);
        }
        bytes += char(0x80);
        unsigned index = 1u << (bpp - 1);
        std::string expected;
        expected += char(index);
        expected += char(index + 1);
        expected += char(index + 2);
        expected += char(index + 128);
        samples.push_back({format, bytes, expected});
    }
    for (const auto &sample : samples)
    {
        CAPTURE(sample.format);
        auto source =
            json{{"w", 1},
                 {"h", 1},
                 {"formatVersion", 9},
                 {"cf", sample.format},
                 {"data", base64_encode((const unsigned char *)sample.bytes.data(), sample.bytes.size())}};
        CGeneration::NormalizePixels(source, false);
        CHECK(base64_decode(source["data"]) == sample.expected);
        CHECK(source["stride"] == 4);
        CHECK(source["cf"] == LV_COLOR_FORMAT_ARGB8888);
    }
    for (auto format : {LV_COLOR_FORMAT_ARGB8888, LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_A8})
    {
        auto source = json{
            {"width", 1}, {"height", 1}, {"format", format}, {"fillColor", 0x123456}, {"fillOpacity", 128}};
        CGeneration::NormalizePixels(source, true);
        CHECK(base64_decode(source["data"]) == std::string("\x56\x34\x12\x80", 4));
    }
    for (auto invalid : {json{{"w", 0}}, json{{"h", 65536}}, json{{"stride", 0}}, json{{"data", "AA=="}}})
    {
        auto source = json{
            {"w", 1}, {"h", 1}, {"formatVersion", 9}, {"cf", LV_COLOR_FORMAT_ARGB8888}, {"data", "AAAA/w=="}};
        source.update(invalid);
        CHECK_THROWS(CGeneration::NormalizePixels(source, false));
    }
}

TEST_CASE("Local image files are embedded once without disturbing device paths", "[targets][assets][io]")
{
    auto directory = std::filesystem::path(UICREATOR_UNIT_OUTPUT) / "image inputs with spaces";
    std::filesystem::create_directories(directory);
    auto source = std::filesystem::path(UICREATOR_SOURCE_ROOT) / "Tests/Fixtures/Images/sample.png";
    std::filesystem::copy_file(source, directory / "shared.png",
                               std::filesystem::copy_options::overwrite_existing);
    json record = {{"kind", "text"}, {"source", "shared.png"}};
    json project = {
        {"obj", json::array({json{{"base", json::object()},
                                  {"imagebutton",
                                   {{"states", json::array({json::array({nullptr, record, nullptr}),
                                                            json::array({nullptr, record, nullptr})})}}}}})}};
    CGeneration::EmbedLocalImages(project, directory);
    REQUIRE(project["images"].size() == 1);
    auto id = project["images"][0]["id"];
    CHECK(project["obj"][0]["imagebutton"]["states"][0][1]["asset"]["projectImage"] == id);
    CHECK(project["obj"][0]["imagebutton"]["states"][1][1]["asset"]["projectImage"] == id);
    auto before = project;
    CGeneration::EmbedLocalImages(project, directory);
    CHECK(project == before);
    json device = {{"obj", json::array()}, {"img", {{"fileName", "Z:/device-only.png"}, {"srcType", 1}}}};
    auto untouched = device;
    CGeneration::EmbedLocalImages(device, directory);
    CHECK(device == untouched);
    json symbol = {{"kind", "text"}, {"source", LV_SYMBOL_OK}};
    auto original = symbol;
    CGeneration::EmbedLocalImages(symbol, directory);
    CHECK(symbol == original);
    {
        std::ofstream file(directory / "unsupported.svg");
        file << "<svg/>";
    }
    json unsupported = {{"kind", "text"}, {"source", "unsupported.svg"}};
    CHECK_THROWS(CGeneration::EmbedLocalImages(unsupported, directory));
}
