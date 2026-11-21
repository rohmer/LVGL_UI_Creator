#include "RuntimeFeatures.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BAR
#include "LVBar.h"

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include "WidgetRecords.h"

namespace Serialization
{
    json LVBar::ToJSON(lv_obj_t* bar)
    {
        auto record = ReadSection(bar, "bar");
        record["range"] = {{"min", lv_bar_get_min_value(bar)}, {"max", lv_bar_get_max_value(bar)}};
        record["value"] = lv_bar_get_value(bar);
        record["startValue"] = lv_bar_get_start_value(bar);
        record["mode"] = lv_bar_get_mode(bar);
        record["orientation"] = lv_bar_get_orientation(bar);
        record["animT"] = lv_obj_get_style_anim_duration(bar, LV_PART_MAIN);
        record["sym"] = lv_bar_get_mode(bar) == LV_BAR_MODE_SYMMETRICAL;
        record["styleBG"] = Style::Read(bar, LV_PART_MAIN, record["styleBG"]);
        record["styleInd"] = Style::Read(bar, LV_PART_INDICATOR, record["styleInd"]);
        return {{"base", LVObject::ToJSON(bar)}, {"bar", record}};
    }

    lv_obj_t* LVBar::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;
        auto* bar = LVObject::FromJSON(j["base"], lv_bar_create(lv_screen_active()));
        const auto record = j["bar"];
        if (!record.is_object())
            return bar;
        PreserveSection(bar, "bar", record);
        auto range = record.value("range", json::object());
        const int minimum = range.value("min", 0);
        const int maximum = range.value("max", 100);
        if (minimum < maximum)
            lv_bar_set_range(bar, minimum, maximum);
        lv_bar_set_mode(bar, static_cast<lv_bar_mode_t>(record.value("mode", record.value("sym", false) ? int(LV_BAR_MODE_SYMMETRICAL) : int(LV_BAR_MODE_NORMAL))));
        lv_bar_set_orientation(bar, static_cast<lv_bar_orientation_t>(record.value("orientation", 0)));
        lv_bar_set_value(bar, record.value("value", minimum), LV_ANIM_OFF);
        if (lv_bar_get_mode(bar) == LV_BAR_MODE_RANGE)
            lv_bar_set_start_value(bar, record.value("startValue", minimum), LV_ANIM_OFF);
        if (!HasNativeStyleSnapshot(j["base"]))
        {
            SetValue(bar, "/bar/animT", record.value("animT", 0));
            Style::Apply(bar, record.value("styleBG", json::object()), LV_PART_MAIN);
            Style::Apply(bar, record.value("styleInd", json::object()), LV_PART_INDICATOR);
        }
        return bar;
    }

    bool LVBar::SetValue(lv_obj_t* obj, std::string key, int value)
    {
        if (key == "/bar/range/max" && value > lv_bar_get_min_value(obj))
            lv_bar_set_range(obj, lv_bar_get_min_value(obj), value);
        else if (key == "/bar/range/min" && value < lv_bar_get_max_value(obj))
            lv_bar_set_range(obj, value, lv_bar_get_max_value(obj));
        else if (key == "/bar/value")
            lv_bar_set_value(obj, value, lv_obj_get_style_anim_duration(obj, LV_PART_MAIN) ? LV_ANIM_ON : LV_ANIM_OFF);
        else if (key == "/bar/animT" && value >= 0)
            lv_obj_set_style_anim_duration(obj, value, LV_PART_MAIN);
        else if (key == "/bar/sym" || key == "/bar/symmetrical")
            lv_bar_set_mode(obj, value ? LV_BAR_MODE_SYMMETRICAL : LV_BAR_MODE_NORMAL);
        else
            return false;
        return true;
    }

    bool LVBar::SetValue(lv_obj_t* obj, std::string key, lv_style_t* value)
    {
        if (!value || (key != "/bar/styleBG" && key != "/bar/styleInd"))
            return false;
        lv_obj_add_style(obj, value, key == "/bar/styleBG" ? LV_PART_MAIN : LV_PART_INDICATOR);
        return true;
    }
}
#else

namespace Serialization
{
    json LVBar::ToJSON(lv_obj_t* bar)
    {
        json j;
        j["base"] = LVObject::ToJSON(bar);

        j["bar"]["range"]["max"] = lv_bar_get_max_value(bar);
        j["bar"]["range"]["min"] = lv_bar_get_min_value(bar);
        j["bar"]["value"] = lv_bar_get_value(bar);
        j["bar"]["animT"] = lv_bar_get_anim_time(bar);
        j["bar"]["sym"] = lv_bar_get_sym(bar);
        auto bgStyle = (lv_style_t*)lv_bar_get_style(bar, LV_BAR_STYLE_BG);
        auto inStyle = (lv_style_t*)lv_bar_get_style(bar, LV_BAR_STYLE_INDIC);

        j["bar"]["styleBG"] = Style::ToJSON(*bgStyle);
        j["bar"]["styleInd"] = Style::ToJSON(*inStyle);
        return j;
    }

    bool LVBar::SetValue(lv_obj_t* obj, std::string key, int value)
    {
        if (key == "/bar/range/max")
        {
            if (value < 1)
                value = 1;
            int min = lv_bar_get_min_value(obj);
            lv_bar_set_range(obj, min, value);
            return true;
        }
        if (key == "/bar/range/min")
        {
            int max = lv_bar_get_max_value(obj);
            lv_bar_set_range(obj, value, max);
            return true;
        }
        if (key == "/bar/value")
        {
            int animT = lv_bar_get_anim_time(obj);
            lv_bar_set_value(obj, value, animT);
        }
        if (key == "/bar/animT")
        {
            lv_bar_set_anim_time(obj, value);
            return true;
        }
        if (key == "/bar/symmetrical")
        {
            lv_bar_set_sym(obj, value);
            return true;
        }
        return true;
    }

    bool LVBar::SetValue(lv_obj_t* obj, std::string key, lv_style_t* value)
    {
        if (key == "/bar/styleBG")
        {
            lv_bar_set_style(obj, LV_BAR_STYLE_BG, value);
            return true;
        }

        if (key == "/bar/styleInd")
        {
            lv_bar_set_style(obj, LV_BAR_STYLE_INDIC, value);
            return true;
        }
        return false;
    }

    lv_obj_t* LVBar::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;

        lv_obj_t* bar = lv_bar_create(lv_scr_act(), nullptr);
        bar = LVObject::FromJSON(j["base"], bar);

        if (j["bar"].is_object())
        {
            json barJ = j["bar"];
            if (barJ["range"].is_object())
            {
                int min = 0, max = 100;
                if (barJ["range"]["max"].is_number())
                {
                    max = barJ["range"]["max"].get<int>();
                }
                if (barJ["range"]["min"].is_number())
                {
                    min = barJ["range"]["min"].get<int>();
                }
                lv_bar_set_range(bar, min, max);
            }
            if (barJ["value"].is_number())
            {
                lv_bar_set_value(bar, barJ["value"], 0);
            }
            if (barJ["sym"].is_boolean())
            {
                lv_bar_set_sym(bar, barJ["sym"]);
            }
            if (barJ["animT"].is_number())
            {
                lv_bar_set_anim_time(bar, barJ["animT"].get<int>());
            }
            if (barJ["styleBG"].is_object())
            {
                const lv_style_t* bgStyle = &Style::FromJSON(barJ["styleBG"]);
                lv_bar_set_style(bar, LV_BAR_STYLE_BG, bgStyle);
            }
            if (barJ["styleInd"].is_object())
            {
                const lv_style_t* inStyle = &Style::FromJSON(barJ["styleInd"]);
                lv_bar_set_style(bar, LV_BAR_STYLE_INDIC, inStyle);
            }
        }
        return bar;
    }
}
#endif

#endif // selected widget
