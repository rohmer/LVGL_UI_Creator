#include "RuntimeFeatures.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LABEL
#include "LVLabel.h"

namespace Serialization
{
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
    namespace
    {
        // These indexes are the LVGL 6 enum values stored in project files.
        constexpr lv_label_long_mode_t longModes[] = {
            LV_LABEL_LONG_MODE_WRAP, LV_LABEL_LONG_MODE_WRAP, LV_LABEL_LONG_MODE_DOTS,
            LV_LABEL_LONG_MODE_SCROLL, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR, LV_LABEL_LONG_MODE_CLIP
        };
        constexpr lv_text_align_t textAlignments[] = {
            LV_TEXT_ALIGN_LEFT, LV_TEXT_ALIGN_CENTER, LV_TEXT_ALIGN_RIGHT
        };
    }
#endif

    json LVLabel::ToJSON(lv_obj_t* label)
    {
        json j;
        j["base"] = LVObject::ToJSON(label);
        std::string text(lv_label_get_text(label));
        j["text"] = text;
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        const auto mode = lv_label_get_long_mode(label);
        if (mode == LV_LABEL_LONG_MODE_WRAP)
            j["longmode"] = lv_obj_get_style_width(label, LV_PART_MAIN) == LV_SIZE_CONTENT ? 0 : 1;
        else
            for (int index = 2; index < 6; ++index)
                if (longModes[index] == mode)
                    j["longmode"] = index;

        const auto align = lv_obj_get_style_text_align(label, LV_PART_MAIN);
        j["textalign"] = align == LV_TEXT_ALIGN_CENTER ? 1 : align == LV_TEXT_ALIGN_RIGHT ? 2 : 0;
        j["drawback"] = lv_obj_get_style_bg_opa(label, LV_PART_MAIN) != LV_OPA_TRANSP;
#else
        j["longmode"] = static_cast<uint8_t>(lv_label_get_long_mode(label));
        j["textalign"] = static_cast<uint8_t>(lv_label_get_align(label));
        j["drawback"] = lv_label_get_body_draw(label);
#endif
        return j;
    }

    lv_obj_t* LVLabel::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        lv_obj_t* label = LVObject::FromJSON(j["base"], lv_label_create(lv_screen_active()));
#else
        lv_obj_t* label = LVObject::FromJSON(j["base"], lv_label_create(lv_scr_act(), nullptr));
#endif

        if (j["text"].is_string())
        {
            lv_label_set_text(label, j["text"].get<std::string>().c_str());
        }

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        if (j["longmode"].is_number_integer())
        {
            const int mode = j["longmode"].get<int>();
            if (mode >= 0 && mode < 6)
            {
                lv_label_set_long_mode(label, longModes[mode]);
                if (mode == 0)
                    lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            }
        }
        // A native snapshot records local styles, including intentional
        // inheritance. Legacy convenience fields must not turn inherited
        // alignment/background into new local overrides on each restore.
        const bool nativeStyles = HasNativeStyleSnapshot(j["base"]);
        if (!nativeStyles && j["textalign"].is_number_integer())
        {
            const int align = j["textalign"].get<int>();
            if (align >= 0 && align < 3 && lv_obj_get_style_text_align(label, LV_PART_MAIN) != textAlignments[align])
                lv_obj_set_style_text_align(label, textAlignments[align], LV_PART_MAIN);
        }
        if (!nativeStyles && j["drawback"].is_boolean())
        {
            const bool drawBackground = j["drawback"].get<bool>();
            if (!drawBackground && lv_obj_get_style_bg_opa(label, LV_PART_MAIN) != LV_OPA_TRANSP)
                lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
            else if (drawBackground && !lv_obj_has_style_prop(label, LV_PART_MAIN, LV_STYLE_BG_OPA))
                lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
        }
#else
        if (j["longmode"].is_number())
        {
            lv_label_set_long_mode(label, j["longmode"].get<uint8_t>());
        }
        if (j["textalign"].is_number())
        {
            lv_label_set_align(label, j["textalign"].get<uint8_t>());
        }
        if (j["drawback"].is_boolean())
        {
            lv_label_set_body_draw(label, j["drawback"].get<bool>());
        }
#endif

        return label;
    }
}

#endif // selected widget
