#include "RuntimeFeatures.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ARC
#include "LVArc.h"
#include "WidgetRecords.h"

namespace Serialization
{
    lv_obj_t* LVArc::Create(lv_obj_t* parent)
    {
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        lv_obj_t* arc = lv_arc_create(parent);
        lv_obj_remove_style(arc, nullptr, LV_PART_KNOB | LV_STATE_ANY);
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
        return arc;
#else
        return lv_arc_create(parent, nullptr);
#endif
    }

    json LVArc::ToJSON(lv_obj_t* arc)
    {
#ifdef LV_RUNTIME_PORTABLE
            auto* saved = static_cast<ObjectUserData*>(lv_obj_get_user_data(arc));
            return saved ? saved->objectJson : json::object();
#else
        json j;
        j["base"] = LVObject::ToJSON(arc);
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        j["arc"] = ReadSection(arc, "arc");
        j["arc"]["angle"]["start"] = lv_arc_get_bg_angle_start(arc);
        j["arc"]["angle"]["end"] = lv_arc_get_bg_angle_end(arc);
        j["arc"]["style"]["line.rounded"] = lv_obj_get_style_arc_rounded(arc, LV_PART_MAIN);
        j["arc"]["style"]["line.width"] = lv_obj_get_style_arc_width(arc, LV_PART_MAIN);
        j["arc"]["style"]["line.color"] = lv_color_to_u32(lv_obj_get_style_arc_color(arc, LV_PART_MAIN));
        j["arc"]["style"]["line.opa"] = lv_obj_get_style_arc_opa(arc, LV_PART_MAIN);
        j["arc"]["value"] = lv_arc_get_value(arc);
        j["arc"]["min"] = lv_arc_get_min_value(arc);
        j["arc"]["max"] = lv_arc_get_max_value(arc);
        j["arc"]["mode"] = lv_arc_get_mode(arc);
        j["arc"]["rotation"] = lv_arc_get_rotation(arc);
        j["arc"]["knobOffset"] = lv_arc_get_knob_offset(arc);
        j["arc"]["changeRate"] = lv_arc_get_change_rate(arc);
        j["arc"]["indicatorStart"] = lv_arc_get_angle_start(arc);
        j["arc"]["indicatorEnd"] = lv_arc_get_angle_end(arc);
#else
        j["arc"]["angle"]["start"] = lv_arc_get_angle_start(arc);
        j["arc"]["angle"]["end"] = lv_arc_get_angle_end(arc);
        const lv_style_t* style = lv_arc_get_style(arc, LV_ARC_STYLE_MAIN);
        bool rounded = false;
        if (style->line.rounded == 1)
            rounded = true;
        j["arc"]["style"]["line.rounded"] = rounded;
        j["arc"]["style"]["line.width"] = style->line.width;
        j["arc"]["style"]["line.color"] = style->line.color.full;
        j["arc"]["style"]["line.opa"] = style->line.opa;
#endif
        return j;
    #endif
        }

    lv_obj_t* LVArc::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;
        lv_obj_t* arc = LVObject::FromJSON(j["base"], Create(lv_scr_act()));
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        const auto& record = j["arc"];
        PreserveSection(arc, "arc", record);
        if (record.contains("min") && record.contains("max"))
            lv_arc_set_range(arc, record.value("min", 0), record.value("max", 100));
        if (record.contains("mode") && record["mode"].get<int>() != lv_arc_get_mode(arc)) lv_arc_set_mode(arc, static_cast<lv_arc_mode_t>(record.value("mode", 0)));
        if (record.contains("value") && record["value"].get<int>() != INT16_MIN) lv_arc_set_value(arc, record.value("value", 0));
        if (record.contains("rotation")) lv_arc_set_rotation(arc, record.value("rotation", 0));
        if (record.contains("knobOffset")) lv_arc_set_knob_offset(arc, record.value("knobOffset", 0));
        if (record.contains("changeRate")) lv_arc_set_change_rate(arc, record.value("changeRate", 720));
#endif

        if (j["arc"]["angle"].is_object())
        {
            int start = 0, end = 0;
            if (j["arc"]["angle"]["start"].is_number())
                start = j["arc"]["angle"]["start"].get<int>();
            if (j["arc"]["angle"]["end"].is_number())
                end = j["arc"]["angle"]["end"].get<int>();
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
            lv_arc_set_bg_angles(arc, start, end);
#else
            lv_arc_set_angles(arc, start, end);
#endif
        }

        if (!HasNativeStyleSnapshot(j["base"]) && j["arc"]["style"].is_object())
        {
            int rounded = 0, lineWidth = 0, opa = 255;
            uint32_t color = 0;
            if (j["arc"]["style"]["line.rounded"].is_boolean() &&
                j["arc"]["style"]["line.rounded"].get<bool>())
                rounded = 1;
            if (j["arc"]["style"]["line.width"].is_number())
                lineWidth = j["arc"]["style"]["line.width"].get<int>();
            if (j["arc"]["style"]["line.color"].is_number())
                color = j["arc"]["style"]["line.color"].get<uint32_t>();
            if (j["arc"]["style"]["line.opa"].is_number())
                opa = j["arc"]["style"]["line.opa"];
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
            lv_obj_set_style_arc_rounded(arc, rounded, LV_PART_MAIN);
            lv_obj_set_style_arc_width(arc, lineWidth, LV_PART_MAIN);
            lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_MAIN);
            lv_obj_set_style_arc_opa(arc, opa, LV_PART_MAIN);
#else
            static lv_style_t style;
            lv_style_copy(&style, &lv_style_plain);
            style.line.color.full = color;
            style.line.rounded = rounded;
            style.line.width = lineWidth;
            style.line.opa = opa;
            lv_arc_set_style(arc, LV_ARC_STYLE_MAIN, &style);
#endif
        }

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        if (record.contains("indicatorStart") && record.contains("indicatorEnd"))
            lv_arc_set_angles(arc, record.value("indicatorStart", 0), record.value("indicatorEnd", 0));
#endif
        return arc;
    }

    bool LVArc::SetValue(lv_obj_t* obj, std::string key, int value)
    {
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        if (key == "/arc/angle/start")
            lv_arc_set_bg_start_angle(obj, value);
        else if (key == "/arc/angle/end")
            lv_arc_set_bg_end_angle(obj, value);
        else if (key == "/arc/style/line.rounded")
            lv_obj_set_style_arc_rounded(obj, value != 0, LV_PART_MAIN);
        else if (key == "/arc/style/line.width")
            lv_obj_set_style_arc_width(obj, value, LV_PART_MAIN);
        else if (key == "/arc/style/line.color")
            lv_obj_set_style_arc_color(obj, lv_color_hex(value), LV_PART_MAIN);
        else if (key == "/arc/style/line.opa")
            lv_obj_set_style_arc_opa(obj, value, LV_PART_MAIN);
        else
            return false;
        return true;
#else
        if (key == "/arc/angle/start")
        {
            int val = lv_arc_get_angle_end(obj);
            lv_arc_set_angles(obj, value, val);
            return true;
        }
        if (key == "/arc/angle/end")
        {
            int val = lv_arc_get_angle_start(obj);
            lv_arc_set_angles(obj, val, value);
            return true;
        }
        if (key == "/arc/style/line.rounded")
        {
            lv_style_t* style = (lv_style_t*)lv_arc_get_style(obj, LV_ARC_STYLE_MAIN);
            style->line.rounded = value;
            lv_arc_set_style(obj, LV_ARC_STYLE_MAIN, style);
            lv_obj_refresh_style(obj);
            return true;
        }
        if (key == "/arc/style/line.width")
        {
            lv_style_t* style = (lv_style_t*)lv_arc_get_style(obj, LV_ARC_STYLE_MAIN);
            style->line.width = value;
            lv_arc_set_style(obj, LV_ARC_STYLE_MAIN, style);
            lv_obj_refresh_style(obj);
            return true;
        }
        if (key == "/arc/style/line.color")
        {
            lv_style_t* style = (lv_style_t*)lv_arc_get_style(obj, LV_ARC_STYLE_MAIN);
            style->line.color = lv_color_hex(value);
            lv_arc_set_style(obj, LV_ARC_STYLE_MAIN, style);
            lv_obj_refresh_style(obj);
            return true;
        }
        if (key == "/arc/style/line.opa")
        {
            lv_style_t* style = (lv_style_t*)lv_arc_get_style(obj, LV_ARC_STYLE_MAIN);
            style->line.opa = value;
            lv_arc_set_style(obj, LV_ARC_STYLE_MAIN, style);
            lv_obj_refresh_style(obj);
            return true;
        }
        return false;
#endif
    }
}

#endif // selected widget
