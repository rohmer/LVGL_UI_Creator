#include "Screen.h"
#include <stdexcept>
namespace Serialization
{
    json Screen::Normalize(const json& record)
    {
        if (!record.is_object()) throw std::invalid_argument("Invalid screen record");
        auto result = record;
        const auto width = record.value("width", 320), height = record.value("height", 240);
        if (width < 1 || height < 1 || width > 32767 || height > 32767) throw std::invalid_argument("Screen dimensions must be between 1 and 32767");
        if (record.contains("base") && !record["base"].is_object()) throw std::invalid_argument("Invalid screen base properties");
        result["width"] = width; result["height"] = height;
        return result;
    }
    json Screen::ToJSON(lv_obj_t* screen, int width, int height)
    {
        if (!screen) throw std::invalid_argument("Screen is null");
        lv_obj_update_layout(screen);
        return Normalize({{"width", width > 0 ? width : lv_obj_get_width(screen)}, {"height", height > 0 ? height : lv_obj_get_height(screen)},
            {"base", LVObject::ToJSON(screen)}});
    }
    lv_obj_t* Screen::FromJSON(const json& record, lv_obj_t* screen)
    {
        const auto data = Normalize(record);
        const bool owns = screen == nullptr;
        if (!screen) screen = lv_obj_create(lv_screen_active());
        try
        {
            if (data.contains("base")) LVObject::FromJSON(data["base"], screen);
            lv_obj_set_size(screen, data.at("width").get<int>(), data.at("height").get<int>());
            lv_obj_update_layout(screen);
            return screen;
        }
        catch (...) { if (owns) lv_obj_delete(screen); throw; }
    }
    Binary Screen::ToProtobuf(lv_obj_t* screen, int width, int height)
    { return Protobuf::Encode(EntityKind::Screen, "screen", ToJSON(screen, width, height)); }
    lv_obj_t* Screen::FromProtobuf(const Binary& input, lv_obj_t* screen)
    {
        const auto record = Protobuf::Decode(input);
        if (record.kind != EntityKind::Screen || record.type != "screen") throw std::invalid_argument("Expected a screen record");
        return FromJSON(record.data, screen);
    }
}
