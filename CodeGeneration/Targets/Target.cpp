#include "Target.h"
#include "Capabilities.h"
#include <Serialization/Base64.h>
#include <fstream>
#include <lvgl/lvgl.h>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
using json = nlohmann::json;
namespace CGeneration
{
void ValidateTarget(const std::string &target)
{
    static const std::set<std::string> versions = {"9.6", "9.5", "9.3", "9.2", "9.1", "8.3"};
    if (!versions.count(target))
        throw std::invalid_argument("Unknown LVGL target: " + target);
}
static void Coordinate(json &value, const std::string &target)
{
    if (!value.is_number_integer())
        return;
    auto number = value.get<int32_t>();
    if (number == 536870911)
        value = {{"coordinateKind", "maximum"}};
    else if (number == -536870911)
        value = {{"coordinateKind", "minimum"}};
    else if (number == 1073741823)
        value = {{"coordinateKind", "content"}};
    else if (((number & 0x60000000) == 0x20000000 && (number & 0x1fffffff) <= 536870910))
    {
        auto percent = ((number & 0x1fffffff) > 268435455 ? 268435455 - (number & 0x1fffffff) : (number & 0x1fffffff));
        if (percent > 1000 || percent < -1000)
            throw std::invalid_argument("LVGL " + target + " percent coordinate exceeds its supported range");
        value = {{"coordinateKind", "percent"}, {"value", percent}};
    }
    else if (target == "8.3" && (number < -16383 || number > 16383))
        throw std::invalid_argument("Pixel coordinate exceeds LVGL 8.3 range");
}
static void Walk(json &value, const json &caps, const std::string &target, bool runtime)
{
    if (value.is_array())
    {
        for (auto &child : value)
            Walk(child, caps, target, runtime);
        return;
    }
    if (!value.is_object())
        return;
    if (value.contains("base") && (value.contains("barcode") || value.contains("qrcode") || value.contains("advancedKeyboard")))
        throw std::invalid_argument("Barcode, QR code and Advanced Keyboard widgets require the bundled LVGL 9.6 target");
    if(runtime && value.contains("deviceFrames")) {
        auto& frames=value["deviceFrames"];
        if(!frames.is_array() || frames.empty())throw std::invalid_argument("Image has no device frames");
        for(auto& frame:frames) {
            if(!frame.is_object() || frame.value("formatVersion",9)!=9)throw std::invalid_argument("Unknown device frame format");
            frame["formatVersion"]=9;
        }
    }
    if (value.contains("flags") && value["flags"].is_object())
    {
        auto &flags = value["flags"];
        for (auto it = flags.begin(); it != flags.end();)
        {
            auto key = it.key();
            for (auto &c : key)
                c = (char)std::toupper((unsigned char)c);
            bool state =
                key == "STATE" || key == "CHECKED" || key == "DISABLED" || key.rfind("STATE_USER_", 0) == 0;
            bool supported =
                state || std::find(caps["flags"].begin(), caps["flags"].end(), key) != caps["flags"].end();
            if (!supported)
            {
                if (it.value() != false && it.value() != 0)
                    throw std::invalid_argument("LVGL " + target + " does not support object flag " +
                                                it.key());
                it = flags.erase(it);
            }
            else
                ++it;
        }
    }
    if (value.contains("nativeStyles"))
        for (auto &style : value["nativeStyles"])
        {
            // The Creator authors LVGL 9.6 state bits. Before 9.5, CHECKED
            // through DISABLED occupied bits 0..7 instead of bits 2..9.
            // C output uses symbolic constants; runtime data needs target bits.
            if(runtime && target!="9.5") {
                const auto selector=style.value("selector",0u);
                const auto state=selector&0xffffu;
                if(state!=0xffffu) {
                    if(state&~0xf3fcu)throw std::invalid_argument("LVGL "+target+" does not support this font/style state");
                    style["selector"]=(selector&0xffff0000u)|(state&0xf000u)|((state&0x3fcu)>>2);
                }
            }
            for (auto it = style["values"].begin(); it != style["values"].end();)
            {
                auto key = it.key();
                for (auto &c : key)
                    c = (char)std::toupper((unsigned char)c);
                if (target == "8.3")
                {
                    auto replace = [](std::string &key, const std::string &a, const std::string &b) {
                        size_t at = key.find(a);
                        if (at != std::string::npos)
                            key.replace(at, a.size(), b);
                    };
                    replace(key, "IMAGE", "IMG");
                    if (key == "ANIM_DURATION")
                        key = "ANIM_TIME";
                }
                if (std::find(caps["styles"].begin(), caps["styles"].end(), key) == caps["styles"].end())
                {
                    if (it.value() != 0 && it.value() != false && it.value() != nullptr)
                        throw std::invalid_argument("LVGL " + target + " does not support style " + it.key());
                    it = style["values"].erase(it);
                }
                else
                    ++it;
            }
        }
    if (target == "8.3" && value.contains("img"))
    {
        auto &image = value["img"];
        if (image.value("scaleX", 256) != image.value("scaleY", 256) || image.value("innerAlign", 0) == 11)
            throw std::invalid_argument(
                "LVGL 8.3 does not support non-uniform image scaling or stretch alignment");
    }
    if (target == "8.3" && value.contains("arc") && value["arc"].value("knobOffset", 0) != 0)
        throw std::invalid_argument("LVGL 8.3 cannot offset an arc knob independently");
    if ((target == "9.1" || target == "8.3") && value.contains("stops"))
    {
        if (value.value("extend", 0) != 0 || value.contains("points"))
            throw std::invalid_argument("LVGL " + target +
                                        " supports only non-extended horizontal/vertical gradients");
        if (target == "8.3")
            for (const auto &stop : value["stops"])
                if (stop.value("opacity", 255) != 255)
                    throw std::invalid_argument("LVGL 8.3 does not support per-stop gradient opacity");
    }
    if (target == "9.1" || target == "8.3")
    {
        if (value.contains("coords") && value["coords"].is_object())
            for (auto &coord : value["coords"].items())
                Coordinate(coord.value(), target);
        if (value.contains("nativeStyles"))
            for (auto &style : value["nativeStyles"])
                for (auto &property : style["values"].items())
                {
                    static const std::set<std::string> coordinates = {
                        "width",   "min_width",  "max_width",   "height",      "min_height", "max_height",
                        "x",       "y",          "translate_x", "translate_y", "pad_left",   "pad_right",
                        "pad_top", "pad_bottom", "pad_row",     "pad_column"};
                    if (coordinates.count(property.key()))
                        Coordinate(property.value(), target);
                    else if (property.key() == "grid_column_dsc_array" ||
                             property.key() == "grid_row_dsc_array")
                        for (auto &track : property.value())
                            if (track.is_number_integer())
                            {
                                auto v = track.get<int32_t>();
                                if (v == 536870810)
                                    track = {{"coordinateKind", "gridContent"}};
                                else if (v >= 536870811 && v < 536870911)
                                    track = {{"coordinateKind", "gridFraction"},
                                             {"value", v - 536870811}};
                            }
                }
    }
    if (value.contains("canvas") && value["canvas"].is_object() &&
        (value["canvas"].contains("width") || value["canvas"].contains("header.w")))
        NormalizePixels(value["canvas"], true);
    if (value.contains("data") && value["data"].is_string() && value.contains("w") && value.contains("h") &&
        value.value("kind", std::string{}) != "encoded")
        NormalizePixels(value, false);
    if (value.contains("deviceFrames") && value.contains("sourceData") && (!runtime || value.contains("previewFrames")))
        value.erase("deviceFrames");
    if ((target == "9.1" || target == "9.2" || target == "8.3") && value.contains("slider") &&
        value["slider"].value("orientation", 0) != 0)
        throw std::invalid_argument("LVGL " + target + " requires automatic slider orientation");
    if ((target == "9.1" || target == "8.3") && value.contains("bar") &&
        value["bar"].value("orientation", 0) != 0)
        throw std::invalid_argument("LVGL " + target + " requires automatic bar orientation");
    if (value.contains("dsc") && value["dsc"].is_object() && value["dsc"].contains("bitmap") &&
        (target == "9.1" || target == "9.2" || target == "8.3"))
    {
        auto &d = value["dsc"];
        unsigned stride = d.value("stride", 0u), bpp = d.value("bpp", 4u);
        if (stride && d.value("bitmapFmt", 0) != 0)
            throw std::invalid_argument("LVGL " + target + " cannot use row-strided compressed fonts");
        if (stride && d.value("bitmapFmt", 0) == 0)
        {
            if (!bpp || bpp > 8)
                throw std::invalid_argument("Invalid font bpp");
            auto input = base64_decode(d.at("bitmap").get<std::string>());
            std::string output;
            for (auto &glyph : d.at("GlyphDsc"))
            {
                auto width = glyph.value("box_w", 0u), height = glyph.value("box_h", 0u);
                auto start = glyph.value("bitmapIdx", 0u);
                auto row = ((width * bpp + 7) / 8 + stride - 1) / stride * stride;
                auto offset = output.size();
                glyph["bitmapIdx"] = offset;
                output.resize(offset + (size_t(width) * height * bpp + 7) / 8, 0);
                for (size_t y = 0; y < height; ++y)
                    for (size_t bit = 0; bit < size_t(width) * bpp; ++bit)
                    {
                        size_t source = size_t(start) + y * row + bit / 8;
                        if (source >= input.size())
                            throw std::invalid_argument("Font bitmap is truncated");
                        if ((unsigned char)input[source] & (0x80 >> (bit % 8)))
                            output[offset + (y * width * bpp + bit) / 8] |=
                                char(0x80 >> ((y * width * bpp + bit) % 8));
                    }
            }
            d["bitmap"] = base64_encode((const unsigned char *)output.data(), output.size());
            d["stride"] = 0;
        }
        if (target == "8.3" && value.value("kerning", 0) == 1)
            d["kernScale"] = 0;
    }
    for (auto &child : value.items())
        Walk(child.value(), caps, target, runtime);
}
void PrepareTarget(json &project, const std::string &target, bool runtime)
{
    ValidateTarget(target);
    if (target == "9.6")
        return;
    auto caps = json::parse(TargetCapabilities).at(target);
    Walk(project, caps, target, runtime);
}
static void Replace(std::string &text, const std::string &before, const std::string &after)
{
    size_t position = 0;
    while ((position = text.find(before, position)) != std::string::npos)
    {
        text.replace(position, before.size(), after);
        position += after.size();
    }
}
void AdaptTarget(std::map<std::string, std::string> &files, const std::string &target)
{
    ValidateTarget(target);
    if (target == "9.6")
        return;
    auto caps = json::parse(TargetCapabilities).at(target);
    auto &compat = files.at("ui_lvgl_compat.h");
    std::regex setters(R"(static inline void lv_obj_set_(\w+)\([^)]*\)\s*\{[^{}]*\}\s*)");
    std::string filtered;
    std::string remaining = compat;
    std::smatch match;
    while (std::regex_search(remaining, match, setters))
    {
        filtered += match.prefix().str();
        auto flag = match[1].str();
        auto key = flag;
        for (auto &c : key)
            c = (char)std::toupper((unsigned char)c);
        bool state =
            key == "STATE" || key == "CHECKED" || key == "DISABLED" || key.rfind("STATE_USER_", 0) == 0;
        if (state || std::find(caps["flags"].begin(), caps["flags"].end(), key) != caps["flags"].end())
            filtered += match[0].str();
        remaining = match.suffix().str();
    }
    compat = filtered + remaining;
    auto manifest = json::parse(files.at("manifest.json"));
    manifest["lvglVersion"] = target;
    manifest["target"] = target;
    files["manifest.json"] = manifest.dump(2);
    auto &assets = files.at("ui_assets.c");
    assets = std::regex_replace(assets, std::regex(",\\.cap_height=[^,}]+,\\.x_height=[^,}]+"), "");
    files["ui.h"].replace(files["ui.h"].find("#include <lvgl.h>"), 17, "#include \"ui_lvgl_compat.h\"");
    Replace(files["ui_assets.h"], "#include <lvgl.h>", "#include \"ui_lvgl_compat.h\"");
    Replace(files["ui_requirements.h"], "LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 6",
            "LVGL_VERSION_MAJOR != " + target.substr(0, 1) + " || LVGL_VERSION_MINOR != " + target.substr(2));
    Replace(files["ui_requirements.h"], "requires LVGL 9.6.x", "requires LVGL " + target + ".x");
    if (target == "9.1" || target == "9.2" || target == "8.3")
    {
        assets = std::regex_replace(assets, std::regex(R"((\.bitmap_format=[^,}]+),\.stride=[^,}]+)"), "$1");
        assets = std::regex_replace(assets, std::regex(R"(,\.static_bitmap=[^,}]+)"), "");
    }
    if (target == "9.1" || target == "8.3")
        files["ui.c"] =
            std::regex_replace(files["ui.c"], std::regex(R"(ui_gradient_([0-9]+)\.extend = [^;]+;)"), "");
    if (target == "8.3")
    {
        files["ui.c"].insert(files["ui.c"].find("#include <math.h>"), "ui_compat_base* ui_compat_states;\n");
        files["ui.c"] =
            std::regex_replace(files["ui.c"], std::regex(R"(static const int32_t (ui_grid_tracks_))"),
                               "static const lv_coord_t $1");
        assets = std::regex_replace(
            assets,
            std::regex(
                R"(\.header=\{\.magic=LV_IMAGE_HEADER_MAGIC,\.cf=([0-9]+),\.w=([0-9]+),\.h=([0-9]+),\.stride=([0-9]+)\})"),
            ".header={.cf=LV_IMG_CF_TRUE_COLOR_ALPHA,.w=$2,.h=$3}");
        assets = std::regex_replace(assets, std::regex(R"(,\.kerning=[^,}]+)"), "");
        files["ui.c"] = std::regex_replace(
            files["ui.c"], std::regex(R"((ui_gradient_[0-9]+\.stops\[[0-9]+\])\.opa = [^;]+;)"), "");
        files["ui_requirements.h"] += "\n#if LV_COLOR_DEPTH != 32\n#error LVGL 8.3 generated image and "
                                      "canvas assets require LV_COLOR_DEPTH 32\n#endif\n";
    }
    Replace(files["README.md"], "9.6.x", target + ".x");
    files["README.md"] += "\nCompatibility target " + target +
                          ". Images are normalized to uncompressed ARGB8888 for these older releases. "
                          "Unsupported nondefault properties fail export explicitly.\n";
}
} // namespace CGeneration
