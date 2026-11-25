#pragma once

#include "WidgetRecords.h"
#include "RuntimeFeatures.h"
#include <algorithm>
#include <vector>

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include "TextInputMode.h"
namespace Serialization
{
    class BasicWidgets
    {
        struct LineData { std::vector<lv_point_precise_t> points; };
        struct TextData { std::string accepted; std::string bullet; std::string text; };

    public:
        static const char* Type(lv_obj_t* object)
        {
            const auto* type = lv_obj_get_class(object);
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_DROPDOWN
            if (type == &lv_dropdown_class) return "dropdown";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ROLLER
            if (type == &lv_roller_class) return "roller";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SLIDER
            if (type == &lv_slider_class) return "slider";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINBOX
            if (type == &lv_spinbox_class) return "spinbox";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SWITCH
            if (type == &lv_switch_class) return "switch";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TEXTAREA
            if (type == &lv_textarea_class) return "textarea";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LED
            if (type == &lv_led_class) return "led";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LINE
            if (type == &lv_line_class) return "line";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINNER
            if (type == &lv_spinner_class) return "spinner";
#endif
            return nullptr;
        }

        static lv_obj_t* Create(const std::string& type, lv_obj_t* parent)
        {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_DROPDOWN
            if (type == "dropdown") return lv_dropdown_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ROLLER
            if (type == "roller") return lv_roller_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SLIDER
            if (type == "slider") return lv_slider_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINBOX
            if (type == "spinbox") return lv_spinbox_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SWITCH
            if (type == "switch") return lv_switch_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TEXTAREA
            if (type == "textarea") return lv_textarea_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LED
            if (type == "led") return lv_led_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LINE
            if (type == "line") return lv_line_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINNER
            if (type == "spinner") return lv_spinner_create(parent);
#endif
            return nullptr;
        }

        static json ToJSON(lv_obj_t* object)
        {
#if defined(LV_RUNTIME_PORTABLE) || LVSERIAL_MINIMAL_RUNTIME
            auto* saved = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
            return saved ? saved->objectJson : json::object();
#else
            lv_obj_update_layout(object);
            const std::string type = Type(object);
            auto data = ReadSection(object, type.c_str());
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_DROPDOWN
            if (type == "dropdown")
            {
                data["options"] = lv_dropdown_get_options(object);
                data["selected"] = lv_dropdown_get_selected(object);
                data["direction"] = lv_dropdown_get_dir(object);
                const char* text = lv_dropdown_get_text(object);
                data["text"] = text ? json(text) : json(nullptr);
                const char* symbol = lv_dropdown_get_symbol(object);
                data["symbol"] = symbol ? json(symbol) : json(nullptr);
                data["highlight"] = lv_dropdown_get_selected_highlight(object);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ROLLER
            if (type == "roller")
            {
                std::string options = lv_roller_get_options(object);
                const auto count = lv_roller_get_option_count(object);
                size_t end = 0;
                for (uint32_t i = 0; i < count; ++i)
                {
                    end = options.find('\n', end);
                    if (end == std::string::npos) break;
                    if (i + 1 < count) ++end;
                }
                const bool infinite = end != std::string::npos;
                if (infinite) options.resize(end);
                data["options"] = options;
                data["selected"] = lv_roller_get_selected(object);
                data["mode"] = infinite ? LV_ROLLER_MODE_INFINITE : LV_ROLLER_MODE_NORMAL;
                const auto* font = lv_obj_get_style_text_font(object, LV_PART_MAIN);
                const int rowHeight = lv_font_get_line_height(font) + lv_obj_get_style_text_line_space(object, LV_PART_MAIN);
                if (!data.contains("visibleRows"))
                    data["visibleRows"] = std::max(1, (lv_obj_get_height(object) - 2 * lv_obj_get_style_border_width(object, LV_PART_MAIN)) / std::max(1, rowHeight));
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SLIDER
            if (type == "slider")
            {
                data["min"] = lv_slider_get_min_value(object);
                data["max"] = lv_slider_get_max_value(object);
                data["value"] = lv_slider_get_value(object);
                data["leftValue"] = lv_slider_get_left_value(object);
                data["mode"] = lv_slider_get_mode(object);
                data["orientation"] = lv_slider_get_orientation(object);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINBOX
            if (type == "spinbox")
            {
                data["min"] = lv_spinbox_get_min_value(object);
                data["max"] = lv_spinbox_get_max_value(object);
                data["value"] = lv_spinbox_get_value(object);
                data["step"] = lv_spinbox_get_step(object);
                data["digits"] = lv_spinbox_get_digit_count(object);
                data["decimalPlaces"] = lv_spinbox_get_dec_point_pos(object);
                data["rollover"] = lv_spinbox_get_rollover(object);
                data["stepDirection"] = lv_spinbox_get_digit_step_direction(object);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SWITCH
            if (type == "switch") data["checked"] = lv_obj_has_state(object, LV_STATE_CHECKED);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TEXTAREA
            if (type == "textarea")
            {
                data["text"] = lv_textarea_get_text(object);
                const char* placeholder = lv_textarea_get_placeholder_text(object);
                data["placeholder"] = placeholder ? placeholder : "";
                const char* accepted = lv_textarea_get_accepted_chars(object);
                data["accepted"] = accepted ? json(accepted) : json(nullptr);
                data["oneLine"] = lv_textarea_get_one_line(object);
                data["password"] = lv_textarea_get_password_mode(object);
                data["passwordBullet"] = lv_textarea_get_password_bullet(object);
                data["passwordShowTime"] = lv_textarea_get_password_show_time(object);
                data["maxLength"] = lv_textarea_get_max_length(object);
                data["cursor"] = lv_textarea_get_cursor_pos(object);
                data["cursorClick"] = lv_textarea_get_cursor_click_pos(object);
                data["selection"] = lv_textarea_get_text_selection(object);
                const auto inputMode = lv_advanced_keyboard_get_textarea_input_mode(object);
                if (inputMode == LV_ADVANCED_KEYBOARD_INPUT_ALPHANUMERIC) data.erase("inputMode");
                else data["inputMode"] = TextInputMode::Name(inputMode);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LED
            if (type == "led")
            {
                data["brightness"] = lv_led_get_brightness(object);
                data["color"] = lv_color_to_u32(lv_led_get_color(object));
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LINE
            if (type == "line")
            {
                data["points"] = json::array();
                const auto* points = lv_line_get_points(object);
                for (uint32_t i = 0; i < lv_line_get_point_count(object); ++i)
                    data["points"].push_back({points[i].x, points[i].y});
                data["invertY"] = lv_line_get_y_invert(object);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINNER
            if (type == "spinner")
            {
                data["duration"] = lv_spinner_get_anim_duration(object);
                data["sweep"] = lv_spinner_get_arc_sweep(object);
            }
#endif
            return {{"base", LVObject::ToJSON(object)}, {type, data}};
        #endif
        }

        static void Apply(lv_obj_t* object, const json& data)
        {
            const std::string type = Type(object);
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TEXTAREA
            if (type == "textarea") TextInputMode::Parse(data.value("inputMode", std::string("alphanumeric")));
#endif
            PreserveSection(object, type.c_str(), data);
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_DROPDOWN
            if (type == "dropdown")
            {
                lv_dropdown_set_options(object, data.value("options", std::string{}).c_str());
                lv_dropdown_set_selected(object, data.value("selected", 0u));
                lv_dropdown_set_dir(object, static_cast<lv_dir_t>(data.value("direction", int(LV_DIR_BOTTOM))));
                auto& strings = WidgetData<TextData>(object);
                const auto symbol = data.value("symbol", json(LV_SYMBOL_DOWN));
                strings.accepted = symbol.is_string() ? symbol.get<std::string>() : "";
                lv_dropdown_set_symbol(object, strings.accepted.empty() ? nullptr : strings.accepted.c_str());
                const auto text = data.find("text");
                strings.text = text != data.end() && text->is_string() ? text->get<std::string>() : "";
                lv_dropdown_set_text(object, text != data.end() && text->is_string() ? strings.text.c_str() : nullptr);
                lv_dropdown_set_selected_highlight(object, data.value("highlight", true));
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ROLLER
            if (type == "roller")
            {
                lv_roller_set_options(object, data.value("options", std::string{}).c_str(),
                    static_cast<lv_roller_mode_t>(data.value("mode", int(LV_ROLLER_MODE_NORMAL))));
                lv_roller_set_visible_row_count(object, std::max(1, data.value("visibleRows", 3)));
                lv_roller_set_selected(object, data.value("selected", 0u), LV_ANIM_OFF);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SLIDER
            if (type == "slider")
            {
                const int min = data.value("min", 0), max = std::max(min, data.value("max", 100));
                lv_slider_set_range(object, min, max);
                lv_slider_set_mode(object, static_cast<lv_slider_mode_t>(data.value("mode", 0)));
                lv_slider_set_orientation(object, static_cast<lv_slider_orientation_t>(data.value("orientation", 0)));
                lv_slider_set_value(object, data.value("value", min), LV_ANIM_OFF);
                if (lv_slider_get_mode(object) == LV_SLIDER_MODE_RANGE)
                    lv_slider_set_left_value(object, data.value("leftValue", min), LV_ANIM_OFF);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINBOX
            if (type == "spinbox")
            {
                const int min = data.value("min", -99999), max = std::max(min, data.value("max", 99999));
                const auto digits = std::clamp(data.value("digits", 5u), 1u, 10u);
                lv_spinbox_set_range(object, min, max);
                lv_spinbox_set_digit_format(object, digits, std::min(data.value("decimalPlaces", 0u), digits));
                lv_spinbox_set_step(object, std::max(1, data.value("step", 1)));
                lv_spinbox_set_value(object, data.value("value", 0));
                lv_spinbox_set_rollover(object, data.value("rollover", false));
                lv_spinbox_set_digit_step_direction(object, static_cast<lv_dir_t>(data.value("stepDirection", int(LV_DIR_RIGHT))));
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SWITCH
            if (type == "switch") lv_obj_set_state(object, LV_STATE_CHECKED, data.value("checked", false));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TEXTAREA
            if (type == "textarea")
            {
                auto& strings = WidgetData<TextData>(object);
                const auto accepted = data.find("accepted");
                strings.accepted = accepted != data.end() && accepted->is_string() ? accepted->get<std::string>() : "";
                strings.bullet = data.value("passwordBullet", std::string("*"));
                lv_textarea_set_accepted_chars(object, accepted != data.end() && accepted->is_string() ? strings.accepted.c_str() : nullptr);
                lv_textarea_set_max_length(object, data.value("maxLength", 0u));
                if (!lv_advanced_keyboard_set_textarea_input_mode(object,
                    TextInputMode::Parse(data.value("inputMode", std::string("alphanumeric")))))
                    throw std::runtime_error("Unable to configure text input mode");
                lv_textarea_set_one_line(object, data.value("oneLine", false));
                lv_textarea_set_password_mode(object, data.value("password", false));
                lv_textarea_set_password_bullet(object, strings.bullet.c_str());
                lv_textarea_set_password_show_time(object, data.value("passwordShowTime", 1500u));
                lv_textarea_set_text(object, data.value("text", std::string{}).c_str());
                lv_textarea_set_placeholder_text(object, data.value("placeholder", std::string{}).c_str());
                lv_textarea_set_cursor_pos(object, data.value("cursor", 0u));
                lv_textarea_set_cursor_click_pos(object, data.value("cursorClick", true));
                lv_textarea_set_text_selection(object, data.value("selection", false));
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LED
            if (type == "led")
            {
                lv_led_set_color(object, lv_color_hex(data.value("color", 0x3399ffu)));
                lv_led_set_brightness(object, std::clamp(data.value("brightness", 255), 0, 255));
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LINE
            if (type == "line")
            {
                auto& points = WidgetData<LineData>(object).points;
                points.clear();
                if (data.contains("points") && data["points"].is_array())
                    for (const auto& point : data["points"])
                        if (point.is_array() && point.size() == 2)
                            points.push_back({point[0].get<lv_value_precise_t>(), point[1].get<lv_value_precise_t>()});
                lv_line_set_points(object, points.data(), static_cast<uint32_t>(points.size()));
                lv_line_set_y_invert(object, data.value("invertY", false));
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINNER
            if (type == "spinner")
                lv_spinner_set_anim_params(object, std::max(1u, data.value("duration", 1000u)),
                    std::clamp(data.value("sweep", 60u), 1u, 360u));
#endif
        }

        static lv_obj_t* FromJSON(const json& record)
        {
            for (const auto* type : {"dropdown", "roller", "slider", "spinbox", "switch", "textarea", "led", "line", "spinner"})
            {
                if (!record.contains(type)) continue;
                auto* object = Create(type, lv_screen_active());
                if (!object) throw std::invalid_argument(std::string("Widget disabled in this runtime: ")+type);
                LVObject::FromJSON(record.at("base"), object);
                Apply(object, record.at(type));
                // Widget setters such as roller visible-row count adjust geometry.
                // Explicit saved base properties remain authoritative on reload.
                LVObject::FromJSON(record.at("base"), object);
                return object;
            }
            return nullptr;
        }
    };
}
#endif
