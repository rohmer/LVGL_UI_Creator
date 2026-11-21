#include "RuntimeFeatures.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTON
#include "LVButton.h"

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include "WidgetRecords.h"
#include "NativeStyles.h"

namespace Serialization
{
    namespace
    {
        struct Caption { lv_obj_t* label = nullptr; };

        lv_obj_t* CaptionLabel(lv_obj_t* button)
        {
            auto& caption = WidgetData<Caption>(button);
            for (uint32_t i = 0; i < lv_obj_get_child_count(button); ++i)
                if (lv_obj_get_child(button, i) == caption.label) return caption.label;
            caption.label = nullptr;
            return nullptr;
        }
        const std::pair<const char*, lv_style_selector_t> states[] = {
            {"rel", LV_STATE_DEFAULT}, {"pr", LV_STATE_PRESSED}, {"ina", LV_STATE_DISABLED},
            {"tglRel", LV_STATE_CHECKED}, {"tglPr", LV_STATE_CHECKED | LV_STATE_PRESSED}
        };
    }

    json LVButton::ToJSON(lv_obj_t* button)
    {
        auto record = ReadSection(button, "button");
        if (auto* label = CaptionLabel(button)) record["text"] = lv_label_get_text(label);
        else record.erase("text");
        record["toggle"] = lv_obj_is_checkable(button);
        record["layout"] = record.value("layout", 0);
        for (const char* side : {"left", "right", "top", "bottom"})
            if (!record["fit"].contains(side)) record["fit"][side] = 0;
        for (const char* stage : {"in", "out", "wait"})
            if (!record["ink"].contains(stage)) record["ink"][stage] = 0;
        for (const auto& state : states)
        {
            auto& style = record["styles"][state.first];
            if (!style.is_object())
                style = Style::Read(button, LV_PART_MAIN);
        }
        return {{"base", LVObject::ToJSON(button)}, {"button", record}};
    }

    lv_obj_t* LVButton::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;
        auto* button = LVObject::FromJSON(j["base"], lv_button_create(lv_screen_active()));
        auto record = j["button"];
        if (!record.is_object())
            return button;
        PreserveSection(button, "button", record);
        if (record.contains("text") && record["text"].is_string()) SetText(button, record["text"].get<std::string>());
        SetValue(button, "/button/toggle", record.value("toggle", false));
        SetValue(button, "/button/layout", record.value("layout", 0));
        if (record["fit"].is_object())
            for (const char* side : {"left", "right", "top", "bottom"})
                if (record["fit"].contains(side))
                    SetValue(button, std::string("/button/fit/") + side, record["fit"][side].get<int>());
        if (!HasNativeStyleSnapshot(j["base"]) && record["styles"].is_object())
            for (const auto& state : states)
                Style::Apply(button, record["styles"][state.first], LV_PART_MAIN | state.second);
        NativeStyles::Apply(button, j["base"].value("nativeStyles", json::array()));
        return button;
    }

    void LVButton::SetText(lv_obj_t* button, const std::string& text)
    {
        auto* label = CaptionLabel(button);
        if (!label) label = WidgetData<Caption>(button).label = lv_label_create(button);
        lv_label_set_text(label, text.c_str());
        lv_obj_center(label);
        auto record = ReadSection(button, "button");
        record["text"] = text;
        PreserveSection(button, "button", record);
    }

    bool LVButton::SetValue(lv_obj_t* obj, std::string key, int value)
    {
        auto record = ReadSection(obj, "button");
        if (key == "/button/toggle")
            lv_obj_set_checkable(obj, value != 0);
        else if (key == "/button/layout" && value >= 0 && value <= 9)
        {
            if (value == 0)
                lv_obj_set_layout(obj, LV_LAYOUT_NONE);
            else
            {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_FLEX
                const bool column = value >= 2 && value <= 4;
                lv_obj_set_flex_flow(obj, column ? LV_FLEX_FLOW_COLUMN :
                                     value >= 8 ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_ROW);
                const auto align = value == 1 || value == 3 || value == 6 ? LV_FLEX_ALIGN_CENTER :
                    value == 4 || value == 7 ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START;
                lv_obj_set_flex_align(obj, value == 1 ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                                      align, LV_FLEX_ALIGN_START);
#else
                throw std::invalid_argument("Button layout requires LV_USE_FLEX");
#endif
            }
            record["layout"] = value;
        }
        else if (key.compare(0, 12, "/button/fit/") == 0 && value >= 0 && value <= 3)
        {
            const auto side = key.substr(12);
            if (side != "left" && side != "right" && side != "top" && side != "bottom")
                return false;
            record["fit"][side] = value;
            if (value != 0)
            {
                const auto size = value == 1 ? LV_SIZE_CONTENT : LV_PCT(100);
                if (side == "left" || side == "right")
                    lv_obj_set_width(obj, size);
                else
                    lv_obj_set_height(obj, size);
            }
        }
        else if (key == "/button/ink/in" || key == "/button/ink/out" || key == "/button/ink/wait")
            record["ink"][key.substr(12)] = value;
        else
            return false;
        PreserveSection(obj, "button", record);
        return true;
    }

    bool LVButton::SetValue(lv_obj_t* obj, std::string key, lv_style_t* style)
    {
        if (!style)
            return false;
        for (const auto& state : states)
        {
            if (key != std::string("/button/styles/") + state.first)
                continue;
            auto record = ReadSection(obj, "button");
            record["styles"][state.first] = Style::ToJSON(*style);
            PreserveSection(obj, "button", record);
            lv_obj_add_style(obj, style, LV_PART_MAIN | state.second);
            return true;
        }
        return false;
    }
}
#else


namespace Serialization
{
    json LVButton::ToJSON(lv_obj_t* button)
    {
        json j;
        j["base"] = LVObject::ToJSON(button);

        auto relStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTN_STYLE_REL);
        auto inStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTN_STYLE_INA);
        auto prStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTN_STYLE_PR);
        auto tglPrStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTN_STYLE_TGL_PR);
        auto tglRelStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTNM_STYLE_BTN_TGL_REL);

        j["button"]["styles"]["rel"] = Style::ToJSON(*relStyle);
        j["button"]["styles"]["ina"] = Style::ToJSON(*inStyle);
        j["button"]["styles"]["pr"] = Style::ToJSON(*prStyle);
        j["button"]["styles"]["tglPr"] = Style::ToJSON(*tglPrStyle);
        j["button"]["styles"]["tglRel"] = Style::ToJSON(*tglRelStyle);
        j["button"]["toggle"] = lv_btn_get_toggle(button);
        j["button"]["layout"] = lv_btn_get_layout(button);
        j["button"]["fit"]["bottom"] = lv_btn_get_fit_bottom(button);
        j["button"]["fit"]["left"] = lv_btn_get_fit_left(button);
        j["button"]["fit"]["top"] = lv_btn_get_fit_top(button);
        j["button"]["fit"]["right"] = lv_btn_get_fit_right(button);
        j["button"]["ink"]["in"] = lv_btn_get_ink_in_time(button);
        j["button"]["ink"]["wait"] = lv_btn_get_ink_wait_time(button);
        j["button"]["ink"]["out"] = lv_btn_get_ink_out_time(button);
        return j;
    }

    bool LVButton::SetValue(lv_obj_t* obj, std::string key, int value)
    {
        if (key == "/button/ink/in")
        {
            lv_btn_set_ink_in_time(obj, value);
            return true;
        }
        if (key == "/button/ink/wait")
        {
            lv_btn_set_ink_wait_time(obj, value);
            return true;
        }
        if (key == "/button/ink/out")
        {
            lv_btn_set_ink_out_time(obj, value);
            return true;
        }
        if (key == "/button/fit/right")
        {
            int bottom = lv_btn_get_fit_bottom(obj);
            int left = lv_btn_get_fit_left(obj);
            int top = lv_btn_get_fit_top(obj);
            int right = lv_btn_get_fit_right(obj);
            lv_btn_set_fit4(obj, left, value, top, bottom);
            return true;
        }
        if (key == "/button/fit/top")
        {
            int bottom = lv_btn_get_fit_bottom(obj);
            int left = lv_btn_get_fit_left(obj);
            int top = lv_btn_get_fit_top(obj);
            int right = lv_btn_get_fit_right(obj);
            lv_btn_set_fit4(obj, left, right, value, bottom);
            return true;
        }
        if (key == "/button/fit/left")
        {
            int bottom = lv_btn_get_fit_bottom(obj);
            int left = lv_btn_get_fit_left(obj);
            int top = lv_btn_get_fit_top(obj);
            int right = lv_btn_get_fit_right(obj);
            lv_btn_set_fit4(obj, value, right, top, bottom);
            return true;
        }
        if (key == "/button/fit/bottom")
        {
            int bottom = lv_btn_get_fit_bottom(obj);
            int left = lv_btn_get_fit_left(obj);
            int top = lv_btn_get_fit_top(obj);
            int right = lv_btn_get_fit_right(obj);
            lv_btn_set_fit4(obj, left, right, top, value);
            return true;
        }
        if (key == "/button/layout")
        {
            lv_btn_set_layout(obj, value);
            return true;
        }
        if (key == "/button/toggle")
        {
            if (value == 1)
                lv_btn_set_toggle(obj, true);
            else
                lv_btn_set_toggle(obj, false);
            return true;
        }
        return false;
    }

    bool LVButton::SetValue(lv_obj_t* obj, std::string key, lv_style_t* style)
    {
        if (key == "/button/styles/tglRel")
        {
            lv_btn_set_style(obj, LV_BTN_STYLE_TGL_REL, style);
            return true;
        }
        if (key == "/button/styles/tglPr")
        {
            lv_btn_set_style(obj, LV_BTN_STYLE_TGL_PR, style);
            return true;
        }
        if (key == "/button/styles/pr")
        {
            lv_btn_set_style(obj, LV_BTN_STYLE_PR, style);
            return true;
        }
        if (key == "/button/styles/ina")
        {
            lv_btn_set_style(obj, LV_BTN_STYLE_INA, style);
            return true;
        }
        if (key == "/button/styles/rel")
        {
            lv_btn_set_style(obj, LV_BTN_STYLE_REL, style);
            return true;
        }
        return false;
    }

    lv_obj_t* LVButton::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;

        lv_obj_t* button = lv_btn_create(lv_scr_act(), nullptr);
        json bj = j["button"];
        if (bj["styles"].is_object())
        {
            if (bj["styles"]["rel"].is_object())
            {
                const lv_style_t* style = &Style::FromJSON(bj["styles"]["rel"]);
                lv_btn_set_style(button, LV_BTN_STYLE_REL, style);
            }
            if (bj["styles"]["ina"].is_object())
            {
                const lv_style_t* style = &Style::FromJSON(bj["styles"]["ina"]);
                lv_btn_set_style(button, LV_BTN_STYLE_INA, style);
            }
            if (bj["styles"]["pr"].is_object())
            {
                const lv_style_t* style = &Style::FromJSON(bj["styles"]["pr"]);
                lv_btn_set_style(button, LV_BTN_STYLE_PR, style);
            }
            if (bj["styles"]["tglPr"].is_object())
            {
                const lv_style_t* style = &Style::FromJSON(bj["styles"]["tglPr"]);
                lv_btn_set_style(button, LV_BTN_STYLE_TGL_PR, style);
            }
            if (bj["styles"]["tglRel"].is_object())
            {
                const lv_style_t* style = &Style::FromJSON(bj["styles"]["tglRel"]);
                lv_btn_set_style(button, LV_BTN_STYLE_TGL_REL, style);
            }
        }
        if (bj["toggle"].is_boolean())
        {
            lv_btn_set_toggle(button, bj["toggle"]);
        }
        if (bj["layout"].is_number())
        {
            lv_btn_set_layout(button, bj["layout"]);
        }
        if (bj["fit"].is_object())
        {
            int b = 0, l = 0, t = 0, r = 0;
            if (bj["fit"]["bottom"].is_number())
            {
                b = bj["fit"]["bottom"];
            }
            if (bj["fit"]["left"].is_number())
            {
                l = bj["fit"]["left"];
            }
            if (bj["fit"]["top"].is_number())
            {
                t = bj["fit"]["top"];
            }
            if (bj["fit"]["right"].is_number())
            {
                r = bj["fit"]["right"];
            }
            lv_btn_set_fit4(button, l, r, t, b);
        }
        if (bj["ink"].is_object())
        {
            if (bj["ink"]["in"].is_number())
                lv_btn_set_ink_in_time(button, bj["ink"]["in"]);
            if (bj["ink"]["wait"].is_number())
                lv_btn_set_ink_wait_time(button, bj["ink"]["wait"]);
            if (bj["ink"]["out"].is_number())
                lv_btn_set_ink_out_time(button, bj["ink"]["out"]);
        }
        return button;
    }
}
#endif

#endif // selected widget
