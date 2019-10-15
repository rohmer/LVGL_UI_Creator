#include "LVArc.h"

namespace Serialization
{
    json LVArc::ToJSON(lv_obj_t* arc)
    {
        json j;
        j["base"] = LVObject::ToJSON(arc);
        j["arc"]["angle"]["start"] = lv_arc_get_angle_start(arc);
        j["arc"]["angle"]["end"] = lv_arc_get_angle_end(arc);
        static const lv_style_t* style = lv_arc_get_style(arc, LV_ARC_STYLE_MAIN);
        bool rounded = false;
        if (style->line.rounded == 1)
            rounded = true;
        j["arc"]["style"]["line.rounded"] = rounded;
        j["arc"]["style"]["line.width"] = style->line.width;
        j["arc"]["style"]["line.color"] = style->line.color.full;
        j["arc"]["style"]["line.opa"] = style->line.opa;
        return j;
    }

    lv_obj_t* LVArc::FromJSON(json j)
    {
        lv_obj_t* arc = lv_arc_create(lv_scr_act(), nullptr);
        if (j["base"].is_object())
        {
            arc = LVObject::FromJSON(j["base"], arc);
        }
        else
        {
            // No base, no object
            return nullptr;
        }

        if (j["arc"]["angle"].is_object())
        {
            int start = 0, end = 0;
            if (j["arc"]["angle"]["start"].is_number())
                start = j["arc"]["angle"]["start"].get<int>();
            if (j["arc"]["angle"]["end"].is_number())
                end = j["arc"]["angle"]["end"].get<int>();
            lv_arc_set_angles(arc, start, end);
        }

        if (j["arc"]["style"].is_object())
        {
            int rounded = 0, lineWidth = 0, color = 0, opa = 255;
            if (j["arc"]["style"]["line.rounded"].is_boolean() &&
                j["arc"]["style"]["line.rounded"].get<bool>())
                rounded = 1;
            if (j["arc"]["style"]["line.width"].is_number())
                lineWidth = j["arc"]["style"]["line.width"].get<int>();
            if (j["arc"]["style"]["line.color"].is_number())
                color = j["arc"]["style"]["line.color"].get<int>();
            if (j["arc"]["style"]["line.opa"].is_number())
                opa = j["arc"]["style"]["line.opa"];
            static lv_style_t style;
            lv_style_copy(&style, &lv_style_plain);
            style.line.color.full = color;
            style.line.rounded = rounded;
            style.line.width = lineWidth;
            style.line.opa = opa;
            lv_arc_set_style(arc, LV_ARC_STYLE_MAIN, &style);
        }

        return arc;
    }

    bool LVArc::SetValue(lv_obj_t* obj, std::string key, int value)
    {
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
    }
}
