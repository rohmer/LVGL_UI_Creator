#include "RuntimeFeatures.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHECKBOX
#include "LVCheckBox.h"

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include "Style.h"
#include "WidgetRecords.h"

namespace Serialization
{
    namespace
    {
        const std::pair<const char*, lv_style_selector_t> selectors[] = {
            {"bg", LV_PART_MAIN}, {"rel", LV_PART_INDICATOR}, {"pr", LV_PART_INDICATOR | LV_STATE_PRESSED},
            {"tglRel", LV_PART_INDICATOR | LV_STATE_CHECKED},
            {"tglPr", LV_PART_INDICATOR | LV_STATE_CHECKED | LV_STATE_PRESSED},
            {"ina", LV_PART_INDICATOR | LV_STATE_DISABLED}
        };
    }

    json LVCheckBox::ToJSON(lv_obj_t* checkbox)
    {
        auto record = ReadSection(checkbox, "cb");
        record["text"] = lv_checkbox_get_text(checkbox);
        record["checked"] = lv_obj_has_state(checkbox, LV_STATE_CHECKED);
        record["inact"] = lv_obj_has_state(checkbox, LV_STATE_DISABLED);
        for (const auto& entry : selectors)
        {
            auto& style = record["style"][entry.first];
            if (!style.is_object())
                style = Style::Read(checkbox, static_cast<lv_part_t>(entry.second & LV_PART_ANY));
        }
        return {{"base", LVObject::ToJSON(checkbox)}, {"cb", record}};
    }

    lv_obj_t* LVCheckBox::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;
        auto* checkbox = LVObject::FromJSON(j["base"], lv_checkbox_create(lv_screen_active()));
        auto record = j["cb"];
        if (!record.is_object())
            return checkbox;
        PreserveSection(checkbox, "cb", record);
        lv_checkbox_set_text(checkbox, record.value("text", std::string{}).c_str());
        lv_obj_set_state(checkbox, LV_STATE_CHECKED, record.value("checked", false));
        lv_obj_set_state(checkbox, LV_STATE_DISABLED, record.value("inact", false));
        if (!HasNativeStyleSnapshot(j["base"]) && record["style"].is_object())
            for (const auto& entry : selectors)
                Style::Apply(checkbox, record["style"][entry.first], entry.second);
        return checkbox;
    }
}
#else

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
                const lv_style_t st = Style::FromJSON(s["bg"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BG, &st);
            }
            if (s["rel"].is_object())
            {
                const lv_style_t st = Style::FromJSON(s["rel"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_REL, &st);
            }
            if (s["ina"].is_object())
            {
                const lv_style_t st = Style::FromJSON(s["ina"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_INA, &st);
            }
            if (s["pr"].is_object())
            {
                const lv_style_t st = Style::FromJSON(s["pr"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_PR, &st);
            }
            if (s["tglRel"].is_object())
            {
                const lv_style_t st = Style::FromJSON(s["tglRel"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_TGL_REL, &st);
            }
            if (s["tglPr"].is_object())
            {
                const lv_style_t st = Style::FromJSON(s["tglPr"]);
                lv_cb_set_style(cb, LV_CB_STYLE_BOX_TGL_PR, &st);
            }

            return cb;
        }
    }
}
#endif

#endif // selected widget
