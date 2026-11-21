#pragma once
#include "LVObject.h"
#include "WidgetRecords.h"
#include <stdexcept>
#include <string>

namespace Serialization
{
// Code data is retained as text in the project instead of serializing the
// generated canvas. Native LVGL owns the encoded bitmap and releases it.
class CodeWidgets
{
public:
    static json Normalize(const std::string& type, const json& input)
    {
        if (type != "barcode" && type != "qrcode") throw std::invalid_argument("Unknown code widget");
        if (!input.is_object()) throw std::invalid_argument("Code widget properties must be an object");
        auto integer = [&](const char* key, int fallback, int minimum, int maximum) {
            const auto found = input.find(key);
            if (found == input.end()) return fallback;
            if (!found->is_number_integer() || (found->is_number_unsigned() && found->get<uint64_t>() > uint64_t(maximum)))
                throw std::invalid_argument(std::string(key) + " must be an integer in its supported range");
            const auto number = found->get<int64_t>();
            if (number < minimum || number > maximum) throw std::invalid_argument(std::string(key) + " is outside its supported range");
            return static_cast<int>(number);
        };
        auto color = [&](const char* key, uint32_t fallback) {
            const auto found = input.find(key);
            if (found == input.end()) return fallback;
            if (!found->is_number_integer() || (found->is_number_integer() && !found->is_number_unsigned() && found->get<int64_t>() < 0) || found->get<uint64_t>() > UINT32_MAX)
                throw std::invalid_argument(std::string(key) + " must be a 32-bit color");
            return 0xff000000u | (found->get<uint32_t>() & 0xffffffu);
        };
        auto record = input;
        record["data"] = input.value("data", std::string(type == "barcode" ? "12345678" : "https://lvgl.io"));
        record["darkColor"] = color("darkColor", 0xff000000u);
        record["lightColor"] = color("lightColor", 0xffffffffu);
        const auto& text = record.at("data").get_ref<const std::string&>();
        if (type == "barcode")
        {
            if (text.empty() || text.size() > 1024) throw std::invalid_argument("Barcode content must contain 1-1024 ASCII bytes");
            for (unsigned char ch : text) if (ch == 0 || ch > 127) throw std::invalid_argument("Code 128 requires ASCII text without NUL bytes");
            record["scale"] = integer("scale", 2, 1, 16);
            record["direction"] = integer("direction", int(LV_DIR_HOR), LV_DIR_HOR, LV_DIR_VER);
            record["tiled"] = input.value("tiled", false);
            record["encoding"] = integer("encoding", 1, 0, 1);
            const auto scale = record.at("scale").get<int>();
            const auto direction = record.at("direction").get<int>();
            const auto encoding = record.at("encoding").get<int>();
            if (scale < 1 || scale > 16) throw std::invalid_argument("Barcode scale must be between 1 and 16 pixels");
            if (direction != LV_DIR_HOR && direction != LV_DIR_VER) throw std::invalid_argument("Barcode direction must be horizontal or vertical");
            if (encoding < 0 || encoding > 1) throw std::invalid_argument("Barcode encoding must be Code 128 GS1 or raw");
        }
        else
        {
            if (text.size() > 2953) throw std::invalid_argument("QR content exceeds 2953 bytes");
            record["size"] = integer("size", 160, 29, 2048);
            record["quietZone"] = input.value("quietZone", true);
            const auto size = record.at("size").get<int>();
            if (size < 29 || size > 2048) throw std::invalid_argument("QR size must be between 29 and 2048 pixels");
        }
        return record;
    }

    static const char* Type(lv_obj_t* object)
    {
        (void)object;
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6
#if LV_USE_BARCODE
        if (lv_obj_get_class(object) == &lv_barcode_class) return "barcode";
#endif
#if LV_USE_QRCODE
        if (lv_obj_get_class(object) == &lv_qrcode_class) return "qrcode";
#endif
#endif
        return nullptr;
    }

    static void Apply(lv_obj_t* object, const json& input)
    {
        const auto* type = Type(object);
        if (!type) throw std::invalid_argument("Code widget support is disabled in this LVGL build");
        const auto next = Normalize(type, input);
        const auto saved = ReadSection(object, type);
        const auto previous = saved.empty() ? saved : ToJSON(object).at(type);
        const auto width = lv_obj_get_style_width(object, LV_PART_MAIN);
        const auto height = lv_obj_get_style_height(object, LV_PART_MAIN);
        // Palette-only edits avoid encoding; unchanged records do no work.
        if (previous == next) return;
        try { Configure(object, type, next, previous); }
        catch (...)
        {
            if (!previous.empty()) try { Configure(object, type, previous, json::object()); } catch (...) {}
            lv_obj_set_size(object, width, height);
            throw;
        }
        PreserveSection(object, type, next);
    }

    static lv_obj_t* Create(const std::string& type, lv_obj_t* parent)
    {
        (void)parent;
        lv_obj_t* object = nullptr;
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6
#if LV_USE_BARCODE
        if (type == "barcode") object = lv_barcode_create(parent);
#endif
#if LV_USE_QRCODE
        if (type == "qrcode") object = lv_qrcode_create(parent);
#endif
#endif
        if (object)
        {
            LVObject::FromJSON(json::object(), object);
            if (type == "barcode") lv_obj_set_size(object, 240, 80);
            else lv_obj_set_size(object, 160, 160);
        }
        return object;
    }

    static json ToJSON(lv_obj_t* object)
    {
        const auto* type = Type(object);
        if (!type) throw std::invalid_argument("Object is not a supported code widget");
        lv_obj_update_layout(object);
        auto record = ReadSection(object, type);
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6 && LV_USE_BARCODE
        if (std::string(type) == "barcode")
        {
            const auto* payload = lv_barcode_get_data(object);
            if (!payload) throw std::invalid_argument("Set barcode data before serializing the widget");
            record["data"] = payload;
            record["scale"] = lv_barcode_get_scale(object);
            record["encoding"] = lv_barcode_get_encoding(object);
            record["direction"] = lv_barcode_get_direction(object);
            record["tiled"] = lv_barcode_get_tiled(object);
            record["darkColor"] = 0xff000000u | lv_color_to_int(lv_barcode_get_dark_color(object));
            record["lightColor"] = 0xff000000u | lv_color_to_int(lv_barcode_get_light_color(object));
        }
#endif
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6 && LV_USE_QRCODE
        if (std::string(type) == "qrcode")
        {
            uint32_t count = 0;
            const auto* payload = static_cast<const char*>(lv_qrcode_get_data(object, &count));
            record["data"] = payload ? std::string(payload, count) : std::string{};
            const auto* buffer = lv_canvas_get_draw_buf(object);
            if (!buffer) throw std::invalid_argument("Set QR data before serializing the widget");
            record["size"] = static_cast<uint32_t>(buffer->header.w);
            record["quietZone"] = lv_qrcode_get_quiet_zone(object);
            record["darkColor"] = 0xff000000u | lv_color_to_int(lv_qrcode_get_dark_color(object));
            record["lightColor"] = 0xff000000u | lv_color_to_int(lv_qrcode_get_light_color(object));
        }
#endif
        record = Normalize(type, record);
        return {{"base", LVObject::ToJSON(object)}, {type, std::move(record)}};
    }

    static lv_obj_t* FromJSON(const json& record)
    {
        const char* type = record.contains("barcode") ? "barcode" : record.contains("qrcode") ? "qrcode" : nullptr;
        if (!type) return nullptr;
        auto* object = Create(type, lv_screen_active());
        if (!object) throw std::invalid_argument(std::string(type) + " requires LVGL 9.6 and its LV_USE_* option");
        try
        {
            LVObject::FromJSON(record.at("base"), object);
            const auto width = lv_obj_get_style_width(object, LV_PART_MAIN);
            const auto height = lv_obj_get_style_height(object, LV_PART_MAIN);
            Apply(object, record.at(type));
            // Saved geometry may intentionally scale the encoded bitmap. A
            // minimal record without geometry uses the requested code size.
            if (std::string(type) == "qrcode")
            {
                const auto& base = record.at("base");
                auto explicitDimension = [&](const char* key) {
                    if (base.contains("coords") && base.at("coords").contains(key)) return true;
                    if (base.contains("nativeStyles")) for (const auto& style : base.at("nativeStyles"))
                        if (style.contains("values") && style.at("values").contains(key)) return true;
                    return false;
                };
                if (explicitDimension("width")) lv_obj_set_width(object, width);
                if (explicitDimension("height")) lv_obj_set_height(object, height);
            }
            return object;
        }
        catch (...) { lv_obj_delete(object); throw; }
    }

private:
    static void Configure(lv_obj_t* object, const std::string& type, const json& record, const json& previous)
    {
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6 && (LV_USE_BARCODE || LV_USE_QRCODE)
        const auto& text = record.at("data").get_ref<const std::string&>();
        auto changed = [&](const char* key) { return !previous.contains(key) || previous.at(key) != record.at(key); };
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6
#if LV_USE_BARCODE
        if (type == "barcode")
        {
            lv_barcode_set_dark_color(object, lv_color_hex(record.at("darkColor").get<uint32_t>()));
            lv_barcode_set_light_color(object, lv_color_hex(record.at("lightColor").get<uint32_t>()));
            const bool encode = changed("data") || changed("encoding") || changed("scale") || changed("direction") || changed("tiled");
            if (encode)
            {
                lv_barcode_set_update_mode(object, LV_BARCODE_UPDATE_MODE_DEFERRED);
                lv_barcode_set_encoding(object, static_cast<lv_barcode_encoding_t>(record.at("encoding").get<int>()));
                lv_barcode_set_scale(object, record.at("scale").get<uint16_t>());
                lv_barcode_set_direction(object, static_cast<lv_dir_t>(record.at("direction").get<int>()));
                lv_barcode_set_tiled(object, record.at("tiled").get<bool>());
                if ((changed("data") && lv_barcode_update(object, text.c_str()) != LV_RESULT_OK) || lv_barcode_render(object) != LV_RESULT_OK)
                    throw std::invalid_argument("Barcode does not fit its dimensions; increase its width/height or reduce scale/content");
                lv_barcode_set_update_mode(object, LV_BARCODE_UPDATE_MODE_IMMEDIATE);
            }
            return;
        }
#endif
#if LV_USE_QRCODE
        if (type == "qrcode")
        {
            lv_qrcode_set_dark_color(object, lv_color_hex(record.at("darkColor").get<uint32_t>()));
            lv_qrcode_set_light_color(object, lv_color_hex(record.at("lightColor").get<uint32_t>()));
            if (changed("data") || changed("size") || changed("quietZone"))
            {
                lv_qrcode_set_update_mode(object, LV_QRCODE_UPDATE_MODE_DEFERRED);
                if (changed("size") || !lv_canvas_get_draw_buf(object))
                {
                    lv_qrcode_set_size(object, record.at("size").get<int>());
                    const auto* buffer = lv_canvas_get_draw_buf(object);
                    if (!buffer || buffer->header.w != record.at("size").get<uint32_t>() || buffer->header.h != record.at("size").get<uint32_t>())
                        throw std::invalid_argument("Cannot allocate the requested QR bitmap");
                    lv_obj_set_size(object, record.at("size").get<int>(), record.at("size").get<int>());
                }
                lv_qrcode_set_quiet_zone(object, record.at("quietZone").get<bool>());
                const auto result = changed("data") ? lv_qrcode_update(object, text.data(), static_cast<uint32_t>(text.size())) : lv_qrcode_render(object);
                if (result != LV_RESULT_OK)
                    throw std::invalid_argument("QR content does not fit its size; increase the size or shorten the content");
                lv_qrcode_set_update_mode(object, LV_QRCODE_UPDATE_MODE_IMMEDIATE);
            }
            return;
        }
#endif
#endif
#else
        (void)object;(void)type;(void)record;(void)previous;
#endif
        throw std::invalid_argument("Code widget support is disabled in this LVGL build");
    }
};
}
