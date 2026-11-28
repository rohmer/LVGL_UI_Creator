#include "MicroPythonCodeGenerator.h"
#include "CCodeGenerator.h"
#include "ProjectAssets.h"
#include "Targets/Target.h"
#include <MicroPythonSource.h>
#include <Serialization/Base64.h>
#include <Serialization/CodeWidgets.h>
#include <Serialization/AdvancedControls.h>
#include <Serialization/ImageAsset.h>
#include <Serialization/ImageRecords.h>
#include <Serialization/InteractionActions.h>
#include <Serialization/LVFont.h>
#include <Serialization/LVVariable.h>
#include <Serialization/ProjectFonts.h>
#include <Serialization/ProjectImages.h>
#include <Serialization/ProjectScreens.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
using json = nlohmann::json;
namespace fs = std::filesystem;
// Pin the requested binding and its own LVGL, rather than combining the
// editor's LVGL headers with an unrelated generated MicroPython module.
constexpr const char* bindingUrl = "https://github.com/lvgl/lv_binding_micropython";
constexpr const char* bindingRevision = "032dda4b7e8419702a4b9c4c8748a4d85d51be61";
constexpr const char* lvglRevision = "c033a98afddd65aaafeebea625382a94020fe4a7";
using Files = std::map<std::string, std::string>;

#if LVGL_VERSION_MAJOR >= 9
struct Exporter {
    fs::path source;
    Files output;
    std::map<std::string, json> imageCache;
    std::map<std::string, std::string> binaryCache;
    std::map<std::string, std::string> fontCache;
    std::set<std::string> requirements;
    json manifest = {{"formatVersion", 1}, {"format", "micropython"}, {"lvglVersion", "9.3.0"},
        {"binding", {{"repository", bindingUrl}, {"revision", bindingRevision}, {"lvglRevision", lvglRevision}}},
        {"screens", json::array()}, {"objects", json::array()}, {"assets", json::array()},
        {"staticCodeWidgets", json::array()}, {"warnings", json::array()}};

    std::string binary(const std::string& bytes) {
        const auto found = binaryCache.find(bytes);
        if (found != binaryCache.end()) return found->second;
        const auto name = "image_" + std::to_string(binaryCache.size() + 1) + ".bin";
        output["assets/" + name] = bytes;
        binaryCache.emplace(bytes, name);
        manifest["assets"].push_back({{"file", "assets/" + name}, {"bytes", bytes.size()}});
        return name;
    }
    json frame(const std::string& raw, uint32_t width, uint32_t height, uint32_t stride,
               uint32_t format, uint32_t flags = 0) {
        if (!width || !height || width > 65535 || height > 65535 || !stride || stride > 65535 || raw.empty())
            throw std::invalid_argument("Invalid MicroPython image dimensions or empty pixel payload");
        if (flags & ~1u) throw std::invalid_argument("MicroPython images must be uncompressed; reimport the source image");
        const auto bpp = lv_color_format_get_bpp(static_cast<lv_color_format_t>(format));
        if (!bpp || stride < (uint64_t(width) * bpp + 7) / 8)
            throw std::invalid_argument("Invalid MicroPython image pixel format or stride");
        auto expected = uint64_t(stride) * height + LV_COLOR_INDEXED_PALETTE_SIZE(format) * 4;
        if (format == LV_COLOR_FORMAT_RGB565A8) expected += uint64_t(stride / 2) * height;
        if (expected != raw.size()) throw std::invalid_argument("Image pixel payload has the wrong size");
        requirements.insert("LV_USE_IMAGE");
        return {{"mpyAsset", binary(raw)}, {"width", width}, {"height", height},
                {"stride", stride}, {"colorFormat", format}, {"flags", flags}};
    }
    json image(const json& record) {
        if (record.is_null() || record.empty()) return record;
        const auto key = record.dump();
        const auto found = imageCache.find(key);
        if (found != imageCache.end()) return found->second;
        json data = record, result;
        if (data.contains("encodedSource")) return image(data.at("encodedSource"));
        if (data.value("kind", std::string{}) == "text") {
            const auto name = data.value("source", std::string{});
            if (name.empty() || static_cast<unsigned char>(name[0]) >= 0x80) return data;
            data = {{"fileName", name}};
        }
        if (data.contains("sym") && !data.contains("asset") && !data.contains("data"))
            return {{"kind", "text"}, {"source", data.at("sym")}};
        if (data.contains("fileName") && !data.contains("asset")) {
            auto path = fs::u8path(data.at("fileName").get<std::string>());
            if (path.is_relative()) path = source / path;
            std::ifstream stream(path, std::ios::binary);
            if (!stream) throw std::runtime_error("Image file not found: " + path.u8string());
            std::string raw((std::istreambuf_iterator<char>(stream)), {});
            data = {{"asset", {{"name", path.filename().u8string()},
                {"sourceData", base64_encode(reinterpret_cast<const unsigned char*>(raw.data()), raw.size())}}}};
        }
        if (data.contains("asset") || data.value("kind", std::string{}) == "encoded") {
            json asset;
            if (data.contains("asset")) asset = data.at("asset");
            else {
                const auto bytes = base64_decode(data.at("data").get<std::string>());
                const auto extension = bytes.size() > 3 && bytes.substr(0, 3) == "GIF" ? "gif" :
                    bytes.size() > 3 && static_cast<unsigned char>(bytes[0]) == 137 ? "png" : "jpg";
                asset = {{"name", std::string("image.") + extension}, {"sourceData", data.at("data")}};
            }
            if (asset.contains("sourceData") || asset.contains("previewFrames")) {
                auto decoded = Serialization::ImageAsset::Decode(asset);
                if (decoded.delays.empty()) decoded.delays.push_back(100);
                const auto size = size_t(decoded.width) * decoded.height * 4;
                if (decoded.pixels.size() != size * decoded.delays.size())
                    throw std::runtime_error("Decoded image frame payload has the wrong size");
                json frames = json::array();
                for (size_t index = 0; index < decoded.delays.size(); ++index)
                    frames.push_back(frame(std::string(reinterpret_cast<const char*>(decoded.pixels.data() + index * size), size),
                        decoded.width, decoded.height, decoded.width * 4, LV_COLOR_FORMAT_ARGB8888));
                result = frames.front();
                if (frames.size() > 1) { result["frames"] = std::move(frames); result["delays"] = decoded.delays; }
            } else if (asset.contains("deviceFrames") && !asset.at("deviceFrames").empty()) {
                json frames = json::array();
                for (const auto& entry : asset.at("deviceFrames")) {
                    const auto raw = base64_decode(entry.at("data").get<std::string>());
                    frames.push_back(frame(raw, entry.at("w"), entry.at("h"), entry.at("stride"),
                        entry.at("cf"), entry.value("flags", 0u)));
                }
                result = frames.front();
                if (frames.size() > 1) { result["frames"] = std::move(frames); result["delays"] = asset.value("delays", json::array()); }
            } else throw std::invalid_argument("Image has no source or device pixels");
        } else if (data.contains("data") && (data.contains("w") || data.contains("header.w"))) {
            CGeneration::NormalizePixels(data, false);
            result = frame(base64_decode(data.at("data").get<std::string>()), data.at("w"), data.at("h"),
                data.at("stride"), data.at("cf"));
        } else return record;
        imageCache.emplace(key, result);
        return result;
    }
    static bool equivalentFont(json left, json right) {
        // These metrics were added after the binding's LVGL. They do not
        // change glyph data in an otherwise identical built-in font.
        for (auto* key : {"capHeight", "xHeight"}) { left.erase(key); right.erase(key); }
        return left == right;
    }
    std::string font(const json& record) {
        if (record.is_null() || record == "default") return "default";
        if (record.is_string()) {
            auto name = record.get<std::string>();
            if (name.rfind("font_", 0) == 0) name.erase(0, 5);
            if (!std::regex_match(name, std::regex("montserrat_[0-9]+")))
                throw std::invalid_argument("Unknown MicroPython built-in font: " + name);
            requirements.insert("LV_FONT_" + upper(name));
            return name;
        }
        const auto signature = record.dump();
        if (fontCache.count(signature)) return fontCache.at(signature);
        if (equivalentFont(record, Serialization::LVFont::ToJSON(*LV_FONT_DEFAULT))) return "default";
        std::vector<std::pair<const lv_font_t*, std::string>> builtins;
#define MPY_FONT(size) builtins.emplace_back(&lv_font_montserrat_##size, "montserrat_" #size)
#if LV_FONT_MONTSERRAT_8
        MPY_FONT(8);
#endif
#if LV_FONT_MONTSERRAT_10
        MPY_FONT(10);
#endif
#if LV_FONT_MONTSERRAT_12
        MPY_FONT(12);
#endif
#if LV_FONT_MONTSERRAT_14
        MPY_FONT(14);
#endif
#if LV_FONT_MONTSERRAT_16
        MPY_FONT(16);
#endif
#if LV_FONT_MONTSERRAT_18
        MPY_FONT(18);
#endif
#if LV_FONT_MONTSERRAT_20
        MPY_FONT(20);
#endif
#if LV_FONT_MONTSERRAT_22
        MPY_FONT(22);
#endif
#if LV_FONT_MONTSERRAT_24
        MPY_FONT(24);
#endif
#if LV_FONT_MONTSERRAT_26
        MPY_FONT(26);
#endif
#if LV_FONT_MONTSERRAT_28
        MPY_FONT(28);
#endif
#if LV_FONT_MONTSERRAT_30
        MPY_FONT(30);
#endif
#if LV_FONT_MONTSERRAT_32
        MPY_FONT(32);
#endif
#if LV_FONT_MONTSERRAT_34
        MPY_FONT(34);
#endif
#if LV_FONT_MONTSERRAT_36
        MPY_FONT(36);
#endif
#if LV_FONT_MONTSERRAT_38
        MPY_FONT(38);
#endif
#if LV_FONT_MONTSERRAT_40
        MPY_FONT(40);
#endif
#if LV_FONT_MONTSERRAT_42
        MPY_FONT(42);
#endif
#if LV_FONT_MONTSERRAT_44
        MPY_FONT(44);
#endif
#if LV_FONT_MONTSERRAT_46
        MPY_FONT(46);
#endif
#if LV_FONT_MONTSERRAT_48
        MPY_FONT(48);
#endif
#undef MPY_FONT
        for (const auto& builtin : builtins) if (equivalentFont(record, Serialization::LVFont::ToJSON(*builtin.first))) {
            requirements.insert("LV_FONT_" + upper(builtin.second));
            fontCache.emplace(signature, builtin.second); return builtin.second;
        }
        throw std::invalid_argument("Custom bitmap fonts require a native MicroPython binding extension; select a built-in font before exporting");
    }
    static std::string upper(std::string value) {
        for (auto& c : value) c = char(std::toupper(static_cast<unsigned char>(c)));
        return value;
    }
    static void coordinate(json& value) {
        if (!value.is_number_integer()) return;
        const auto number = value.get<int32_t>();
        if (number == LV_SIZE_CONTENT) value = {{"content", true}};
        else if (LV_COORD_IS_PCT(number)) value = {{"percent", LV_COORD_GET_PCT(number)}};
    }
    void styles(json& base) {
        if (base.contains("accessibility") && base["accessibility"].contains("tabIndex"))
            throw std::invalid_argument("Explicit focus tabIndex needs application-owned MicroPython input-group setup; remove the authored tabIndex before exporting");
        if (base.contains("events")) base["events"] = Serialization::InteractionActions::Normalize(base.at("events"));
        if (base.contains("coords")) for (auto& property : base["coords"].items()) coordinate(property.value());
        if (base.contains("style") && base["style"].contains("text") && base["style"]["text"].contains("font"))
            base["style"]["text"]["font"] = font(base["style"]["text"]["font"]);
        if (!base.contains("nativeStyles")) return;
        for (auto& style : base["nativeStyles"]) for (auto& property : style["values"].items()) {
            auto& value = property.value(); const auto& name = property.key();
            if (name == "text_font") value = font(value);
            else if (name == "bg_image_src" || name == "arc_image_src" || name == "bitmap_mask_src") value = image(value);
            else if (name == "grid_column_dsc_array" || name == "grid_row_dsc_array") {
                requirements.insert("LV_USE_GRID");
                if (!value.empty() && value.back() == LV_GRID_TEMPLATE_LAST) value.erase(value.end() - 1);
                for (auto& track : value) if (track.is_number_integer()) {
                    const auto v = track.get<int32_t>();
                    if (v == LV_GRID_CONTENT) track = {{"gridContent", true}};
                    else if (v == LV_GRID_TEMPLATE_LAST) throw std::invalid_argument("Grid end marker must be the last track");
                    else if (v >= LV_GRID_FR(0) && v < LV_GRID_TEMPLATE_LAST) track = {{"gridFr", v - LV_GRID_FR(0)}};
                    else coordinate(track);
                }
            } else if (name == "bg_grad") {
                if (value.value("direction", 0) > 2 || value.value("extend", 0) != 0 || value.contains("points") ||
                    value.value("stops", json::array()).size() > 2)
                    throw std::invalid_argument("MicroPython export supports only two-stop horizontal/vertical gradients");
            } else {
                if (name.rfind("flex_", 0) == 0) requirements.insert("LV_USE_FLEX");
                static const std::set<std::string> coordinates = {"width","min_width","max_width","height","min_height","max_height",
                    "x","y","translate_x","translate_y","pad_left","pad_right","pad_top","pad_bottom","pad_row","pad_column"};
                if (coordinates.count(name)) coordinate(value);
            }
        }
    }
    json codeImage(const char* type, const json& record, const json& base) {
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6
        const bool started = !lv_is_initialized();
        if (started) lv_init();
        auto* oldDisplay = lv_display_get_default();
        lv_display_t* display = nullptr;
        lv_obj_t* object = nullptr;
        try {
            display = lv_display_create(320, 240);
            if (!display) throw std::runtime_error("Cannot create code export display");
            lv_display_set_default(display);
            object = Serialization::CodeWidgets::Create(type, lv_screen_active());
            if (!object) throw std::runtime_error("The editor has no code encoder for MicroPython bitmap export");
            if (std::string(type) == "barcode") {
                auto check = [](const json& value) {
                    if (!value.is_number_integer() || !LV_COORD_IS_PX(value.get<int32_t>()))
                        throw std::invalid_argument("Set barcode width/height to pixel dimensions before MicroPython bitmap export");
                };
                if (base.contains("coords")) for (auto* key : {"width", "height"})
                    if (base["coords"].contains(key)) check(base["coords"][key]);
                for (const auto& style : base.value("nativeStyles", json::array()))
                    if (style.value("selector", 0) == 0) for (auto* key : {"width", "height"})
                        if (style.at("values").contains(key)) check(style["values"][key]);
            }
            Serialization::LVObject::FromJSON(base, object);
            lv_obj_update_layout(object);
            Serialization::CodeWidgets::Apply(object, record);
            const auto* buffer = lv_canvas_get_draw_buf(object);
            if (!buffer || !buffer->data) throw std::runtime_error("Code encoder produced no image");
            // A draw buffer may reserve aligned/reusable capacity beyond its
            // active pixels. Export only the palette and active image planes.
            auto payload = uint64_t(buffer->header.stride) * buffer->header.h +
                LV_COLOR_INDEXED_PALETTE_SIZE(buffer->header.cf) * 4;
            if (buffer->header.cf == LV_COLOR_FORMAT_RGB565A8)
                payload += uint64_t(buffer->header.stride / 2) * buffer->header.h;
            if (payload > buffer->data_size) throw std::runtime_error("Code encoder returned a truncated image buffer");
            auto result = frame(std::string(reinterpret_cast<const char*>(buffer->data), static_cast<size_t>(payload)),
                buffer->header.w, buffer->header.h, buffer->header.stride, buffer->header.cf, buffer->header.flags & 1u);
            result["innerAlign"] = lv_image_get_inner_align(object);
            lv_obj_delete(object); object = nullptr;
            lv_display_delete(display); display = nullptr;
            if (oldDisplay) lv_display_set_default(oldDisplay);
            if (started) lv_deinit();
            return result;
        } catch (...) {
            if (object) lv_obj_delete(object);
            if (display) lv_display_delete(display);
            if (oldDisplay) lv_display_set_default(oldDisplay);
            if (started) lv_deinit();
            throw;
        }
#else
        (void)type; (void)record; (void)base;
        throw std::invalid_argument("MicroPython code bitmap export requires the editor's LVGL 9.6 encoder");
#endif
    }
    void objects(json& value, size_t screen) {
        if (value.is_array()) { for (auto& item : value) objects(item, screen); return; }
        if (!value.is_object()) return;
        if (!value.contains("base") && (value.contains("asset") ||
            value.value("kind", std::string{}) == "text" || value.value("kind", std::string{}) == "encoded" ||
            (value.contains("data") && (value.contains("w") || value.contains("header.w"))))) {
            value = image(value); return;
        }
        if (value.contains("text") && value["text"].is_object() && value["text"].contains("font"))
            value["text"]["font"] = font(value["text"]["font"]);
        if (value.contains("advancedKeyboard"))
            throw std::invalid_argument("Advanced Keyboard requires the custom C++ widget; the official MicroPython binding does not expose it. Use a standard Keyboard or C/Protobuf export");
        if (value.contains("textBox") || value.contains("layerView"))
            throw std::invalid_argument("The Text box and Layer view composite widgets require C/Protobuf export; replace them with native LVGL controls for MicroPython");
        for(const auto* type:{"radioGroup","checkboxButton","labeledCheckbox","datePicker","menu"})
            if(value.contains(type))value[type]=Serialization::AdvancedControls::Normalize(type,value.at(type));
        if (value.contains("spinner"))
            throw std::invalid_argument("The pinned official MicroPython binding does not expose lv.spinner; use an Arc or C/Protobuf export");
        if (value.contains("keyboard")) {
            if (!value["keyboard"].is_object())
                throw std::invalid_argument("Keyboard settings must be an object");
            const auto mode = value["keyboard"].value("mode", json(0));
            if (!mode.is_number_integer() || (mode.is_number_unsigned() ? mode.get<uint64_t>() > 8 :
                (mode.get<int64_t>() < 0 || mode.get<int64_t>() > 8)))
                throw std::invalid_argument("Keyboard mode must be an integer from 0 through 8");
            if (mode >= 4 && mode <= 7) {
                const auto buttons = value["keyboard"].value("buttons", json::object());
                const auto map = buttons.is_object() ? buttons.value("btnMap", json::array()) : json::array();
                const bool assigned = map.is_array() && std::any_of(map.begin(), map.end(), [](const json& label) {
                    return label.is_string() && !label.get_ref<const std::string&>().empty() && label != "\n";
                });
                if (!assigned) throw std::invalid_argument("Keyboard user mode requires a nonempty custom buttons.btnMap");
            }
            if (mode == 8) requirements.insert("LV_USE_ARABIC_PERSIAN_CHARS");
        }
        if (value.contains("cal") && value["cal"].contains("monthN")) {
            const json defaults = {"January","February","March","April","May","June","July","August","September","October","November","December"};
            const auto& months = value["cal"]["monthN"];
            if (!months.empty() && months != defaults)
                throw std::invalid_argument("Custom calendar month titles require a native extension; use the default month names for MicroPython export");
        }
        if (value.contains("nativeStyles")) styles(value);
        if (value.contains("base")) {
            for (auto* type : {"barcode", "qrcode"}) if (value.contains(type)) {
                const auto snapshot = codeImage(type, value.at(type), value.at("base"));
                manifest["staticCodeWidgets"].push_back({{"screen", screen}, {"oid", value.value("oid", -1)}, {"type", type}});
                value.erase(type); value["img"] = snapshot;
                break;
            }
            styles(value["base"]);
        }
        if (value.contains("img")) {
            auto normalized = image(value.at("img"));
            if (normalized.contains("mpyAsset") || normalized.value("kind", std::string{}) == "text") {
                auto& img = value["img"];
                for (auto* key : {"asset","data","fileName","encodedSource","w","h","header.w","header.h","cf","formatVersion"}) img.erase(key);
                img.update(normalized);
            }
            auto& img = value["img"];
            // LVGL 9.6 inserted CONTAIN_DOWNSCALE before COVER. Passing the
            // numeric value through would silently select a different mode.
            if (img.value("innerAlign", 0) == 14)
                throw std::invalid_argument("Contain-downscale image alignment requires LVGL 9.6; use Contain or Cover for MicroPython");
            if (img.value("innerAlign", 0) == 15) img["innerAlign"] = 14;
            if (value["img"].contains("style")) objects(value["img"]["style"], screen);
        }
        for (auto& item : value.items()) {
            if (item.key() == "base" || item.key() == "img") continue;
            if (item.key() == "style" && item.value().is_object() && item.value().contains("text") && item.value()["text"].contains("font"))
                item.value()["text"]["font"] = font(item.value()["text"]["font"]);
            if (item.key() == "encodedSource") item.value() = image(item.value());
            else if (item.value().is_object() && (item.value().contains("asset") ||
                item.value().value("kind", std::string{}) == "text" || item.value().value("kind", std::string{}) == "encoded" ||
                (item.value().contains("data") && item.value().contains("w")))) item.value() = image(item.value());
            else objects(item.value(), screen);
        }
    }
    void canvasAssets(json& value) {
        if (value.is_array()) { for (auto& item : value) canvasAssets(item); return; }
        if (!value.is_object()) return;
        if (value.contains("canvas") && value["canvas"].is_object() && value["canvas"].contains("data")) {
            auto& canvas = value["canvas"];
            canvas["mpyAsset"] = binary(base64_decode(canvas.at("data").get<std::string>()));
            canvas.erase("data");
        }
        for (auto& item : value.items()) canvasAssets(item.value());
    }
    Files files(json prepared, const std::string& directory, json& summary) {
        prepared = Serialization::ProjectScreens::Device(std::move(prepared));
        CGeneration::EmbedLocalImages(prepared, source);
        Serialization::ProjectFonts::References(prepared, prepared.value("fonts", json::array()), true);
        Serialization::ProjectImages::References(prepared, prepared.value("images", json::array()), true);
        if (!prepared.contains("screens")) prepared["screens"] = json::array({{
            {"name", "Screen 1"}, {"canvas", prepared.value("canvas", json{{"width",320},{"height",240}})},
            {"obj", prepared.value("obj", json::array())}}});
        prepared.erase("obj"); prepared.erase("canvas");
        for (size_t si = 0; si < prepared["screens"].size(); ++si) {
            auto& screen = prepared["screens"][si];
            int next = 0;
            for (auto& record : screen["obj"]) { if (!record.contains("oid")) record["oid"] = next; ++next; }
            if (screen.contains("canvas") && screen["canvas"].contains("base") && screen["canvas"]["base"].contains("coords")) {
                screen["canvas"]["base"]["coords"]["x"] = 0;
                screen["canvas"]["base"]["coords"]["y"] = 0;
            }
            objects(screen, si);
            manifest["screens"].push_back({{"index", si}, {"name", screen.value("name", "Screen")}});
        }
        // The C exporter remains the shared structural/variable validation
        // authority. Code widgets were already encoded above; validate the
        // original project separately in the public entry points.
        // A screen's canvas describes its size and styles, not a canvas widget
        // with a pixel buffer. Keep it out of the widget pixel adapter so a
        // plain screen does not generate an unused full-size framebuffer.
        std::vector<json> screenCanvases;
        for (auto& screen : prepared["screens"]) {
            screenCanvases.push_back(screen.contains("canvas") ? std::move(screen["canvas"]) : json());
            screen.erase("canvas");
            CGeneration::PrepareTarget(screenCanvases.back(), "9.3");
        }
        CGeneration::PrepareTarget(prepared, "9.3");
        canvasAssets(prepared);
        for (size_t index = 0; index < screenCanvases.size(); ++index)
            if (!screenCanvases[index].is_null()) prepared["screens"][index]["canvas"] = std::move(screenCanvases[index]);
        for (auto* key : {"images","fonts","tries","components","componentLibrary","themes","selectedTheme","editorScreen","version","formatVersion","projectId"}) prepared.erase(key);
        json compact = {{"formatVersion", 1}, {"activeScreen", prepared.value("activeScreen", 0)},
            {"screens", std::move(prepared["screens"])}, {"variables", Serialization::LVVariable::NormalizeList(prepared.value("variables", json::array()))}};
        for (auto& record : compact["variables"]) if (record.contains("var")) {
            auto& variable = record["var"];
            if (variable.contains("code")) {
                if (variable["code"].contains("cType")) variable["code"] = {{"cType", variable["code"].at("cType")}};
                else variable.erase("code");
            }
            for (auto* key : {"codeSymbol","codeType","cppSymbol","cType","codeStorage"}) variable.erase(key);
        }
        auto resources = CGeneration::MicroPythonSources();
        if (!resources.count("uicreator_lvgl.py")) throw std::runtime_error("The MicroPython runtime was not bundled; rebuild the editor");
        output.insert(resources.begin(), resources.end());
        output["ui.json"] = compact.dump() + "\n";
        output["ui.py"] = R"PY(# Generated by LVGL UI Creator. Keep application code in a separate module.
from uicreator_lvgl import UI


def create(project_path=None, asset_root=None):
    # Resolve files relative to this module, including when imported from /lib.
    base = globals().get("__file__", "ui.py").replace("\\", "/").rsplit("/", 1)
    base = base[0] + "/" if len(base) == 2 else ""
    app = UI(project_path or base + "ui.json", asset_root or base + "assets")
    app.load()
    return app
)PY";
        output["main.py"] = R"PY(# Configure your display, input devices and LVGL event loop before creating UI.
# See README.md. Retain display/input/event-loop references for their lifetime.
import lvgl as lv
import ui


def start():
    # The binding initializes LVGL on import; board code creates the display.
    if lv.display_get_default() is None:
        raise RuntimeError("Initialize your LVGL display and event loop before main.start()")
    return ui.create()


# In your board's startup module, after initializing display/input/event loop:
# app = start()
# app.set_variable("temperature", 21)
# widget = app.get_object(0, 1)
)PY";
        std::ostringstream config;
        config << "/* Merge these options into the official binding's lv_conf.h.\n"
                  " * This is a requirements fragment, not a replacement: preserve GC,\n"
                  " * MicroPython allocators and display-driver settings from upstream. */\n";
        for (const auto& macro : requirements) config << "#define " << macro << " 1\n";
        output["lv_conf.defaults.h"] = config.str();
        manifest["requiredConfiguration"] = requirements;
        manifest["warnings"].push_back("The application supplies display/input drivers and the LVGL event loop; no board pins are assumed.");
        if (!manifest["staticCodeWidgets"].empty()) manifest["warnings"].push_back("Barcode and QR widgets are exported as encoded bitmap images; re-export to change their content.");
        manifest["warnings"].push_back("Application C/C++ symbols are replaced by Python variable accessors; attach hardware logic from your own Python module.");
        output["README.md"] = readme();
        manifest["files"] = json::array();
        size_t size = 0;
        for (const auto& item : output) { size += item.second.size(); manifest["files"].push_back({{"file",item.first},{"bytes",item.second.size()}}); }
        output["export-manifest.json"] = manifest.dump(2) + "\n";
        size += output.at("export-manifest.json").size();
        summary = {{"target", "MicroPython / LVGL 9.3.0"}, {"files", output.size()}, {"sourceBytes", size},
            {"manifest", manifest}, {"requirements", config.str()}, {"directory", directory}};
        return std::move(output);
    }
    std::string readme() const {
        return std::string(R"MD(# Generated MicroPython LVGL UI

These sources use the official LVGL MicroPython binding:
https://github.com/lvgl/lv_binding_micropython

The exporter targets the binding's LVGL **9.3.0**, submodule revision
`c033a98afddd65aaafeebea625382a94020fe4a7`. Use the binding's LVGL submodule,
not the editor's LVGL 9.6. There is no second LVGL source copy in this export.
The binding source revision is recorded in `export-manifest.json`.

```sh
git clone https://github.com/lvgl/lv_binding_micropython.git
git -C lv_binding_micropython checkout 032dda4b7e8419702a4b9c4c8748a4d85d51be61
git -C lv_binding_micropython submodule update --init --recursive
```

## Firmware and configuration

The upstream quick-start firmware project is https://github.com/lvgl/lv_micropython.
Follow that project's instructions for your MicroPython port and board. The
binding can also be integrated into an existing MicroPython build using its
`micropython.cmake`/`micropython.mk`; initialize its LVGL and parser submodules.
Consult the binding's README for the firmware build and supported drivers.
Standard MicroPython firmware without `import lvgl` support cannot run this UI.

Merge `lv_conf.defaults.h` into the binding's configuration before rebuilding the
firmware. Preserve upstream's MicroPython GC/allocator settings. The file lists
only the UI's widget/layout/font requirements; board and display-driver settings
remain the application's responsibility. Rebuilding the binding regenerates its
Python API for enabled modules. The export itself needs no C/C++ or Protobuf.

## Upload and run

Copy `ui.py`, `ui.json`, `uicreator_lvgl.py` and the complete `assets/` directory
to the board (for example with MicroPython's `mpremote fs cp`). `main.py` is a
startup example, not a panel driver. Do not replace a working board startup
module without integrating its display initialization.

After initializing LVGL, your display/input drivers and the LVGL event loop:

```python
import ui
app = ui.create()
app.set_variable("temperature", 21)  # Use a variable defined by your project.
value = app.get_variable("temperature")
widget = app.get_object(0, 1)       # (screen index, authored widget ID)
app.show_screen(0)
```

Retain `app` and the display/input/event-loop objects. All UI calls run on the
same MicroPython/LVGL thread. The pinned binding initializes LVGL on import.
For its Unix SDL build, configure `lv.sdl_window_create(width, height)`, input devices and
`lv_utils.event_loop()` first. Embedded display pins and bus settings are
configured by your chosen upstream driver. The UI does not create a display or
start a duplicate timer/event loop.

`app.deinit()` releases generated screens, timers and Python asset references;
call it before deleting the display. Re-exporting replaces generated files.
The runtime uses standard MicroPython array and json/ujson modules. LVGL stores
custom keyboard layouts globally by mode, so their latest backing buffers stay
cached per mode after deinit; live UIs retain borrowed layout snapshots.
Keep application code outside this directory. `ui.json` contains only runtime
project records, not editor sessions, saved build targets or source paths.

## Supported features and limits

Native LVGL 9.3 widgets, layout/style data, screen navigation, variables,
multiple widget bindings and authored interaction actions use Python APIs.
Radio Group, Checkbox, Checkbox nested in Label, Date Picker and Menu use native
LVGL Python composites. Date Picker uses a compact button matrix and a popup
created only on first use, with selectable first weekday, month navigation,
Gregorian leap dates, optional date limits and ISO-backed display formatting.
Menu supports nested pages, disabled items and a Back button. Read or set their
values with `get_advanced_value(widget)` and `set_advanced_value(widget, value)`;
use `get_advanced_part(widget, name)` for named native parts. Their root emits
VALUE_CHANGED for user edits. Values also support the normal variable bindings.
`open_date_picker(widget)` and `close_date_picker(widget)` control the popup.
C/C++ variable symbols are authoring hints; update the corresponding variable
with `set_variable` from application Python. PNG/JPEG/GIF files are decoded at
export; deduplicated raw pixel files require no device-side image codec.
Animated frames use one LVGL timer and retain their backing buffers. Budget
MicroPython heap for objects, pixel data and configured variable histories.

Barcode and QR widgets are baked to image assets with the editor's encoder.
Their exact content, colors, quiet zone and orientation are preserved, and no
native barcode/QR binding is required. Change their data in the editor and
re-export to regenerate the bitmap.

Advanced Keyboard, Text box and Layer view custom C++ widgets require C or
Protobuf export. Custom bitmap fonts need a native binding extension; select a
built-in font for this export. The pinned binding does not expose the Spinner
class; use an Arc or native C/Protobuf output for that widget. Unsupported
nondefault LVGL 9.6 styles/flags and
complex gradients are rejected during preflight, before writing output files.
Authored focus `tabIndex` and custom calendar month titles also need native
application integration and are rejected. Message-box close buttons hide the
owned dialog so its registered object/variable references remain valid until
`app.deinit()`.
The runtime checks enabled widget/method capabilities rather than silently
substituting another widget. Hardware display/touch operation is not tested by
a successful source export.
)MD");
    }
};

Files generate(const json& document, const std::string& source, const std::string& directory, json& summary) {
    CCodeGenerator validator;
    validator.SetSourceDirectory(source);
    if (!validator.LoadProject(document)) throw std::invalid_argument(validator.GetLastError());
    json validated;
    if (!validator.Preflight(validated)) throw std::invalid_argument(validator.GetLastError());
    Exporter exporter; exporter.source = fs::u8path(source);
    const auto& requirements = validated.at("requirements").get_ref<const std::string&>();
    const std::regex macro("LV_USE_[A-Z0-9_]+");
    for (std::sregex_iterator it(requirements.begin(), requirements.end(), macro), end; it != end; ++it) {
        const auto name = it->str();
        static const std::set<std::string> baked = {"LV_USE_BARCODE", "LV_USE_QRCODE", "LV_USE_ADVANCED_KEYBOARD",
            "LV_USE_LODEPNG", "LV_USE_TJPGD", "LV_USE_LIBPNG", "LV_USE_LIBJPEG_TURBO", "LV_USE_GIF",
            "LV_USE_LZ4", "LV_USE_LZ4_INTERNAL", "LV_USE_RLE"};
        if (!baked.count(name)) exporter.requirements.insert(name);
    }
    return exporter.files(document, directory, summary);
}
#endif
}

bool MicroPythonCodeGenerator::Preflight(nlohmann::json& summary) {
    summary = nlohmann::json::object();
    try {
#if LVGL_VERSION_MAJOR >= 9
        generate(project, sourceDirectory, {}, summary);
        lastError.clear(); return true;
#else
        throw std::runtime_error("MicroPython export requires an LVGL 9 editor build");
#endif
    } catch (const std::exception& error) { lastError = error.what(); return false; }
}

bool MicroPythonCodeGenerator::ExportProject(std::string directory) {
    try {
        if (directory.empty()) throw std::invalid_argument("Choose an output directory");
#if LVGL_VERSION_MAJOR >= 9
        json summary;
        auto files = generate(project, sourceDirectory, directory, summary); // Complete validation before any writes.
        const auto root = fs::u8path(directory); fs::create_directories(root);
        std::vector<std::string> previousAssets;
        {
            std::ifstream previous(root / "export-manifest.json", std::ios::binary);
            if (previous) {
                const auto manifest = json::parse(previous, nullptr, false);
                if (manifest.is_object() && manifest.contains("format") && manifest["format"] == "micropython" &&
                    manifest.contains("formatVersion") && manifest["formatVersion"].is_number_integer() && manifest["formatVersion"] == 1 &&
                    manifest.contains("files") && manifest["files"].is_array()) {
                    const std::regex generatedAsset(R"(^assets/image_[0-9]+\.bin$)");
                    for (const auto& entry : manifest["files"])
                        if (entry.is_object() && entry.contains("file") && entry["file"].is_string()) {
                            const auto name = entry["file"].get<std::string>();
                            if (!files.count(name) && std::regex_match(name, generatedAsset)) previousAssets.push_back(name);
                        }
                }
            }
        }
        for (const auto& file : files) {
            const auto target = root / fs::u8path(file.first);
            fs::create_directories(target.parent_path());
            auto temporary = target; temporary += ".uicreator-tmp";
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            stream.write(file.second.data(), static_cast<std::streamsize>(file.second.size())); stream.close();
            if (!stream) throw std::runtime_error("Cannot write " + target.u8string());
            std::error_code error; fs::rename(temporary, target, error);
            if (error) { fs::remove(target); fs::rename(temporary, target); }
        }
        // Remove only assets owned by the previous successful export. Keep
        // unlisted application files and never follow a stale asset symlink
        // or an assets directory redirected outside the selected output.
        const auto assetDirectory = fs::weakly_canonical(root) / "assets";
        for (const auto& name : previousAssets) {
            const auto target = root / fs::u8path(name);
            std::error_code error;
            const auto status = fs::symlink_status(target, error);
            if (error || !fs::is_regular_file(status)) continue;
            if (fs::weakly_canonical(target).parent_path() == assetDirectory) fs::remove(target);
        }
        lastError.clear(); return true;
#else
        throw std::runtime_error("MicroPython export requires an LVGL 9 editor build");
#endif
    } catch (const std::exception& error) { lastError = error.what(); return false; }
}
