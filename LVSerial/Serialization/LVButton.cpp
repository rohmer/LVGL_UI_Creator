#include "LVButton.h"


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
