#include "LVCheckBox.h"

namespace Serialization
{
    json LVCheckBox::ToJSON(lv_obj_t* cb)
    {
        json j;
        j["base"] = LVObject::ToJSON(cb);
        j["cb"]["text"] = lv_cb_get_text(cb);
        j["cb"]["checked"] = lv_cb_is_checked(cb);
        j["cb"]["inact"] = lv_cb_is_inactive(cb);
        lv_style_t* bg = (lv_style_t*)lv_cb_get_style(cb, LV_CB_STYLE_BG);
        lv_style_t* rel = (lv_style_t*)lv_cb_get_style(cb, LV_CB_STYLE_BOX_REL);
        lv_style_t* pr = (lv_style_t*)lv_cb_get_style(cb, LV_CB_STYLE_BOX_PR);
        lv_style_t* tglRel = (lv_style_t*)lv_cb_get_style(cb, LV_CB_STYLE_BOX_REL);
        lv_style_t* tglPr = (lv_style_t*)lv_cb_get_style(cb, LV_CB_STYLE_BOX_TGL_PR);
        lv_style_t* ina = (lv_style_t*)lv_cb_get_style(cb, LV_CB_STYLE_BOX_INA);

        j["cb"]["style"]["bg"] = Style::ToJSON(*bg);
        j["cb"]["style"]["rel"] = Style::ToJSON(*rel);
        j["cb"]["style"]["pr"] = Style::ToJSON(*pr);
        j["cb"]["style"]["tglRel"] = Style::ToJSON(*tglRel);
        j["cb"]["style"]["tglPr"] = Style::ToJSON(*tglPr);
        j["cb"]["style"]["ina"] = Style::ToJSON(*ina);
        return j;
    }

    lv_obj_t* LVCheckBox::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;

        lv_obj_t* cb = lv_cb_create(lv_scr_act(), nullptr);
        cb = LVObject::FromJSON(j["base"], cb);

        if (j["cb"]["text"].is_string())
            lv_cb_set_text(cb, j["cb"]["text"].get<std::string>().c_str());
        if (j["cb"]["checked"].is_number())
            lv_cb_set_checked(cb, j["cb"]["checked"]);
        if (j["cb"]["inact"].is_boolean() && j["cb"]["inact"] == true)
            lv_cb_set_inactive(cb);
        if (j["cb"]["style"].is_object())
        {
            json s = j["cb"]["style"];
            if (s["bg"].is_object())
            {
                const lv_style_t* st = &Style::FromJSON(s["bg"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BG, st);
            }
            if (s["rel"].is_object())
            {
                const lv_style_t* st = &Style::FromJSON(s["rel"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_REL, st);
            }
            if (s["ina"].is_object())
            {
                const lv_style_t* st = &Style::FromJSON(s["ina"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_INA, st);
            }
            if (s["pr"].is_object())
            {
                const lv_style_t* st = &Style::FromJSON(s["pr"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_PR, st);
            }
            if (s["tglRel"].is_object())
            {
                const lv_style_t* st = &Style::FromJSON(s["tglRel"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_TGL_REL, st);
            }
            if (s["tglPr"].is_object())
            {
                const lv_style_t* st = &Style::FromJSON(s["tglPr"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_TGL_PR, st);
            }

            return cb;
        }
    }
}
