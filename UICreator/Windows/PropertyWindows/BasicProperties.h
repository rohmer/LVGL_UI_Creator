#pragma once
#include "../../Utils/EditorStyle.h"
#include "../PropertyWindow.h"
#include "../../Utils/EditorControls.h"
#include <Serialization/BasicWidgets.h>
#include <cctype>
#include <charconv>
#include <cstdio>

#if LVGL_VERSION_MAJOR >= 9
class BasicProperties
{
    struct Binding
    {
        PropertyWindow* window;
        std::string section;
        std::string key;
        json original;
        std::vector<int> choices;
    };

    static void Change(lv_event_t* event)
    {
        auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
        auto* control = lv_event_get_target_obj(event);
        if (lv_event_get_code(event) == LV_EVENT_DELETE) { delete binding; return; }
        auto* object = binding->window->GetSelectedObject();
        if (!object || binding->window->Drawing()) return;
        if (lv_event_get_code(event) == LV_EVENT_CLICKED)
        {
            auto* group = binding->window->GetKBGroup();
            FocusEditorControl(group, control);
            return;
        }
        if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
        json value;
        EditorEdit::Scope edit(lv_event_get_target_obj(event),lv_obj_get_class(lv_event_get_target_obj(event))==&lv_textarea_class);
        try
        {
            if (!binding->choices.empty()) value = binding->choices.at(lv_dropdown_get_selected(control));
            else if (binding->original.is_boolean()) value = lv_obj_has_state(control, LV_STATE_CHECKED);
            else if (binding->original.is_string()) value = lv_textarea_get_text(control);
            else if (binding->key == "color")
            {
                const std::string text = lv_textarea_get_text(control);
                uint32_t color = 0;
                if (text.size() != 7 || text[0] != '#') return;
                const auto parsed = std::from_chars(text.data() + 1, text.data() + text.size(), color, 16);
                if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return;
                value = color;
            }
            else value = json::parse(lv_textarea_get_text(control));
            if (binding->original.is_number() && !value.is_number()) return;
            auto record = Serialization::ObjectSerializer::SerializeObject(object);
            const auto path = binding->section.empty() ? "/" + binding->key : "/" + binding->section + "/" + binding->key;
            record[json::json_pointer(path)] = value;
            if (Serialization::BasicWidgets::Type(object))
                Serialization::BasicWidgets::Apply(object, record[binding->section]);
            else if (lv_obj_get_class(object) == &lv_label_class)
            {
                lv_label_set_text(object, record["text"].get<std::string>().c_str());
                lv_obj_set_style_text_align(object, static_cast<lv_text_align_t>(record.value("textalign", 0)), LV_PART_MAIN);
                constexpr lv_label_long_mode_t modes[] = {LV_LABEL_LONG_MODE_WRAP, LV_LABEL_LONG_MODE_WRAP,
                    LV_LABEL_LONG_MODE_DOTS, LV_LABEL_LONG_MODE_SCROLL, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR, LV_LABEL_LONG_MODE_CLIP};
                lv_label_set_long_mode(object, modes[std::clamp(record.value("longmode", 0), 0, 5)]);
                lv_obj_set_style_bg_opa(object, record.value("drawback", false) ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
            }
            else if (lv_obj_get_class(object) == &lv_checkbox_class)
            {
                const auto& data = record["cb"];
                lv_checkbox_set_text(object, data.value("text", std::string{}).c_str());
                lv_obj_set_state(object, LV_STATE_CHECKED, data.value("checked", false));
                lv_obj_set_state(object, LV_STATE_DISABLED, data.value("inact", false));
            }
            else if (lv_obj_get_class(object) == &lv_image_class)
            {
                if (value.is_boolean()) Serialization::LVImage::SetValue(object, "/img/" + binding->key, value.get<bool>() ? 1 : 0);
                else if (value.is_number_integer()) Serialization::LVImage::SetValue(object, "/img/" + binding->key, value.get<int>());
                else if (binding->key == "sym" || binding->key == "fileName") lv_image_set_src(object, value.get<std::string>().c_str());
            }
            auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
            data->objectJson = Serialization::ObjectSerializer::SerializeObject(object);
            lv_obj_set_style_border_color(control, lv_color_hex(EditorStyle::Border), LV_PART_MAIN);
        }
        catch (const json::exception&)
        {
            lv_obj_set_style_border_color(control, lv_color_hex(EditorStyle::Error), LV_PART_MAIN);
        }
    }

    static void Field(PropertyWindow* window, lv_obj_t* parent, const std::string& section,
                      const std::string& key, const json& value)
    {
        if (value.is_object())
        {
            if (section == "img" && key == "pivot")
                for (const auto& coordinate : value.items()) Field(window, parent, section, key + "/" + coordinate.key(), coordinate.value());
            return;
        }
        if (value.is_null() || key == "formatVersion" || key == "rawData" || key == "data" || key == "style" ||
            (section == "img" && (key == "srcType" || key == "cf" || key == "w" || key == "h" || key == "stride"))) return;
        std::string name;
        for (char character : key)
        {
            if (std::isupper(static_cast<unsigned char>(character))) name += ' ';
            name += character == '/' ? ' ' : character;
        }
        if (!name.empty()) name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        lv_obj_t* control;
        std::vector<int> choices;
        const char* options = nullptr;
        if (section == "slider" && key == "mode") { options = "Normal\nSymmetrical\nRange"; choices = {0, 1, 2}; }
        else if (section == "slider" && key == "orientation") { options = "Automatic\nHorizontal\nVertical"; choices = {0, 1, 2}; }
        else if (section == "roller" && key == "mode") { options = "Normal\nInfinite"; choices = {0, 1}; }
        else if (section == "dropdown" && key == "direction") { options = "Bottom\nTop\nLeft\nRight"; choices = {LV_DIR_BOTTOM, LV_DIR_TOP, LV_DIR_LEFT, LV_DIR_RIGHT}; }
        else if (section == "spinbox" && key == "stepDirection") { options = "Left\nRight"; choices = {LV_DIR_LEFT, LV_DIR_RIGHT}; }
        else if (section.empty() && key == "textalign") { options = "Left\nCenter\nRight"; choices = {0, 1, 2}; }
        else if (section.empty() && key == "longmode") { options = "Expand\nWrap\nEllipsis\nScroll\nCircular scroll\nClip"; choices = {0, 1, 2, 3, 4, 5}; }
        if (value.is_boolean())
        {
            control = lv_checkbox_create(parent);
            lv_checkbox_set_text(control, name.c_str());
            lv_obj_set_state(control, LV_STATE_CHECKED, value.get<bool>());
        }
        else
        {
            auto* field = lv_cont_create(parent, nullptr);
            lv_obj_set_size(field, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(field, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_bg_opa(field, LV_OPA_TRANSP, LV_PART_MAIN);
            auto* label = lv_label_create(field);
            lv_label_set_text(label, name.c_str());
            if (options)
            {
                control = lv_dropdown_create(field);
                lv_dropdown_set_options(control, options);
                const auto selected = std::find(choices.begin(), choices.end(), value.get<int>());
                if (selected != choices.end()) lv_dropdown_set_selected(control, static_cast<uint32_t>(selected - choices.begin()));
            }
            else control = lv_textarea_create(field);
            lv_obj_set_width(control, LV_PCT(100));
            const bool multiline = value.is_array() || (value.is_string() && value.get<std::string>().find('\n') != std::string::npos);
            if (!options) lv_textarea_set_one_line(control, !multiline);
            lv_obj_set_height(control, multiline ? 90 : 36);
            lv_obj_set_style_pad_all(control, 8, LV_PART_MAIN);
            lv_obj_set_style_bg_color(control, lv_color_hex(EditorStyle::Input), LV_PART_MAIN);
            lv_obj_set_style_border_color(control, lv_color_hex(EditorStyle::Border), LV_PART_MAIN);
            auto text = value.is_string() ? value.get<std::string>() : value.dump();
            if (key == "color" && value.is_number_unsigned())
            {
                char color[8];
                std::snprintf(color, sizeof(color), "#%06x", value.get<uint32_t>() & 0xffffff);
                text = color;
            }
            if (!options) lv_textarea_set_text(control, text.c_str());
            if (value.is_number() && !options) lv_textarea_set_accepted_chars(control, key == "color" ? "#0123456789abcdefABCDEF" : "-0123456789.");
        }
        auto* binding = new Binding{window, section, key, value, std::move(choices)};
        lv_obj_add_event_cb(control, Change, LV_EVENT_ALL, binding);
    }

public:
    static void Update(PropertyWindow* window, const json& record)
    {
        window->Drawing(true);
        auto* panel = window->ObjectPropWin();
        panel->DeleteChildren();
        panel->UpdateHeight(LV_SIZE_CONTENT);
        auto* content = lv_cont_create(panel->GetWindow(), nullptr);
        lv_obj_set_size(content, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
        panel->AddObjectToWindow(content);
        for (auto entry = record.begin(); entry != record.end(); ++entry)
        {
            if (entry.key() == "base") continue;
            if (entry.value().is_object())
                for (auto field = entry.value().begin(); field != entry.value().end(); ++field)
                    Field(window, content, entry.key(), field.key(), field.value());
            else Field(window, content, "", entry.key(), entry.value());
        }
        window->CurrentlyLoadedProp = PropertyWindow::NONE;
        panel->SetVisible(lv_obj_get_child_count(content) != 0);
        window->Drawing(false);
    }
};
#endif
