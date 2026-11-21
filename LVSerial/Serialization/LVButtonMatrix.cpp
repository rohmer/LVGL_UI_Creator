#include "RuntimeFeatures.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTONMATRIX
#include "LVButtonMatrix.h"

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include <string_view>
#include "Style.h"
#include "WidgetRecords.h"

namespace Serialization
{
    namespace
    {
        const lv_style_selector_t buttonStates[] = {
            LV_PART_ITEMS, LV_PART_ITEMS | LV_STATE_PRESSED, LV_PART_ITEMS | LV_STATE_CHECKED,
            LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_PRESSED, LV_PART_ITEMS | LV_STATE_DISABLED
        };

        struct ButtonMap
        {
            std::vector<std::string> labels;
            std::vector<const char*> pointers;

            explicit ButtonMap(std::vector<std::string> entries) : labels(std::move(entries))
            {
                for (const auto& label : labels)
                    pointers.push_back(label.c_str());
                pointers.push_back("");
            }

            static void Destroy(lv_event_t* event)
            {
                delete static_cast<ButtonMap*>(lv_event_get_user_data(event));
            }
        };

        uint32_t ButtonCount(lv_obj_t* object)
        {
            uint32_t count = 0;
            for (auto map = lv_buttonmatrix_get_map(object); map && *map && **map; ++map)
                if (std::string_view(*map) != "\n") ++count;
            return count;
        }

        bool IsRecolored(lv_obj_t* object)
        {
            return ButtonCount(object) && lv_buttonmatrix_has_button_ctrl(object, 0, LV_BUTTONMATRIX_CTRL_RECOLOR);
        }
    }

    uint16_t LVButtonMatrix::ReadControl(lv_obj_t* object, uint32_t index)
    {
        uint16_t result = 0;
        for (uint16_t bit = 1; bit <= 4; bit <<= 1)
            if (lv_buttonmatrix_has_button_ctrl(object, index, static_cast<lv_buttonmatrix_ctrl_t>(bit)))
                result |= bit;
        for (uint16_t bit = 8; bit <= 256; bit <<= 1)
            if (lv_buttonmatrix_has_button_ctrl(object, index, static_cast<lv_buttonmatrix_ctrl_t>(bit << 1)))
                result |= bit;
        return result;
    }

    json LVButtonMatrix::ToJSON(lv_obj_t* object)
    {
        auto record = ReadSection(object, "btnm");
        if (record.empty() && lv_obj_get_class(object) == &lv_keyboard_class)
            record = ReadSection(object, "keyboard").value("buttons", json::object());
        record["btnMap"] = json::array();
        record["ctrl"] = json::array();
        record["nativeCtrl"] = json::array();
        uint32_t index = 0;
        for (auto map = lv_buttonmatrix_get_map(object); map && *map && **map; ++map)
        {
            record["btnMap"].push_back(*map);
            if (std::string_view(*map) != "\n")
            {
                uint32_t native = 0;
                for (uint32_t flag = 1; flag <= 0x8000; flag <<= 1)
                    if (lv_buttonmatrix_has_button_ctrl(object, index, static_cast<lv_buttonmatrix_ctrl_t>(flag))) native |= flag;
                record["nativeCtrl"].push_back(native);
                record["ctrl"].push_back(ReadControl(object, index++));
            }
        }
        record["oneToggle"] = lv_buttonmatrix_get_one_checked(object);
        record["recolor"] = IsRecolored(object);
        record["selected"] = lv_buttonmatrix_get_selected_button(object);
        for (std::size_t i = 0; i < 5; ++i)
        {
            auto& style = record["btnStyles"][i];
            if (!style.is_object())
                style = Style::Read(object, LV_PART_ITEMS);
        }
        return {{"base", LVObject::ToJSON(object)}, {"btnm", record}};
    }

    lv_obj_t* LVButtonMatrix::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;
        auto* object = LVObject::FromJSON(j["base"], lv_buttonmatrix_create(lv_screen_active()));
        auto record = j["btnm"];
        if (!record.is_object())
            return object;
        Apply(object, record, !HasNativeStyleSnapshot(j["base"]));
        return object;
    }

    void LVButtonMatrix::Apply(lv_obj_t* object, json record, bool applyStyles)
    {
        auto previous = ReadSection(object, "btnm");
        if (previous.empty() && lv_obj_get_class(object) == &lv_keyboard_class)
            previous = ReadSection(object, "keyboard").value("buttons", json::object());
        PreserveSection(object, "btnm", record);
        if (record["btnMap"].is_array())
            SetValue(object, "/btnm/btnMap", record["btnMap"].get<std::vector<std::string>>());
        if (record["ctrl"].is_array())
            SetValue(object, "/btnm/ctrl", record["ctrl"].get<std::vector<uint16_t>>());
        SetValue(object, "/btnm/oneToggle", record.value("oneToggle", false));
        SetValue(object, "/btnm/recolor", record.value("recolor", false));
        if (record.contains("nativeCtrl") && record["nativeCtrl"].is_array())
            for (uint32_t i = 0; i < record["nativeCtrl"].size() && i < ButtonCount(object); ++i)
            {
                lv_buttonmatrix_clear_button_ctrl(object, i, static_cast<lv_buttonmatrix_ctrl_t>(0xffff));
                lv_buttonmatrix_set_button_ctrl(object, i, static_cast<lv_buttonmatrix_ctrl_t>(record["nativeCtrl"][i].get<uint32_t>()));
            }
        if (record.contains("recolor") && previous.contains("recolor") && previous["recolor"] != record["recolor"])
            SetValue(object, "/btnm/recolor", record.value("recolor", false));
        if (record.contains("selected")) lv_buttonmatrix_set_selected_button(object, record["selected"].get<uint32_t>());
        if (applyStyles && record["btnStyles"].is_array())
            for (std::size_t i = 0; i < std::min<std::size_t>(5, record["btnStyles"].size()); ++i)
                Style::Apply(object, record["btnStyles"][i], buttonStates[i]);
    }

    bool LVButtonMatrix::SetValue(lv_obj_t* object, std::string key, int value)
    {
        if (key == "/btnm/oneToggle")
            lv_buttonmatrix_set_one_checked(object, value != 0);
        else if (key == "/btnm/recolor" && value != 0)
            lv_buttonmatrix_set_button_ctrl_all(object, LV_BUTTONMATRIX_CTRL_RECOLOR);
        else if (key == "/btnm/recolor")
            lv_buttonmatrix_clear_button_ctrl_all(object, LV_BUTTONMATRIX_CTRL_RECOLOR);
        else
            return false;
        return true;
    }

    bool LVButtonMatrix::SetValue(lv_obj_t* object, std::string key, lv_style_t* style)
    {
        if (!style)
            return false;
        const char* names[] = {"btnRel", "btnPr", "tglRel", "tglPr", "ina"};
        for (std::size_t i = 0; i < 5; ++i)
        {
            if (key != std::string("/btnm/btnStyles/") + names[i] &&
                key != std::string("/btnm/btnStyles/") + std::to_string(i))
                continue;
            auto record = ReadSection(object, "btnm");
            record["btnStyles"][i] = Style::ToJSON(*style);
            PreserveSection(object, "btnm", record);
            lv_obj_add_style(object, style, buttonStates[i]);
            return true;
        }
        return false;
    }

    bool LVButtonMatrix::SetValue(lv_obj_t* object, std::string key, std::vector<std::string> value)
    {
        if (key != "/btnm/btnMap" || value.empty())
            return false;
        auto* map = new ButtonMap(std::move(value));
        const bool recolor = IsRecolored(object);
        lv_buttonmatrix_set_map(object, map->pointers.data());
#if LVGL_VERSION_MAJOR == 8
        if(auto* old=static_cast<ButtonMap*>(lv_obj_get_event_user_data(object, ButtonMap::Destroy))) {
            lv_obj_remove_event_cb_with_user_data(object,ButtonMap::Destroy,old);delete old;
        }
#else
        for (uint32_t i = 0; i < lv_obj_get_event_count(object); ++i)
        {
            auto* event = lv_obj_get_event_dsc(object, i);
            if (lv_event_dsc_get_cb(event) == ButtonMap::Destroy)
            {
                delete static_cast<ButtonMap*>(lv_event_dsc_get_user_data(event));
                lv_obj_remove_event_dsc(object, event);
                break;
            }
        }
#endif
        lv_obj_add_event_cb(object, ButtonMap::Destroy, LV_EVENT_DELETE, map);
        SetValue(object, "/btnm/recolor", recolor);
        return true;
    }

    bool LVButtonMatrix::SetValue(lv_obj_t* object, std::string key, std::vector<uint16_t> value)
    {
        if (key != "/btnm/ctrl")
            return false;
        const auto count = ButtonCount(object);
        value.resize(count, 1);
        std::vector<lv_buttonmatrix_ctrl_t> controls;
        const auto recolor = IsRecolored(object) ? LV_BUTTONMATRIX_CTRL_RECOLOR : 0;
        for (auto bits : value)
            controls.push_back(static_cast<lv_buttonmatrix_ctrl_t>((bits & 7u) | ((bits & 0x1f8u) << 1) | recolor));
        if (!controls.empty())
            lv_buttonmatrix_set_ctrl_map(object, controls.data());
        return true;
    }
}
#else

namespace Serialization
{
    json LVButtonMatrix::ToJSON(lv_obj_t* btnm)
    {
        json j;
        j["base"] = LVObject::ToJSON(btnm);
        lv_btnm_ext_t* ext = static_cast<lv_btnm_ext_t*>(lv_obj_get_ext_attr(btnm));
        int btnCnt = ext->btn_cnt;
        int buttonCt = ext->btn_cnt;
        const char** ma = lv_btnm_get_map_array(btnm);
        int ctr = 0;
        int row = 0;
        while (*ma)
        {
            const char* p = *ma;
            if(p)
            while (strlen(p) > 0)
            {
                j["btnm"]["btnMap"][ctr] = p;
                p++;
                ctr++;
            }
            ma++;
        }
        for (int i = 0; i < buttonCt * sizeof(lv_btnm_ctrl_t); i++)
        {
            j["btnm"]["ctrl"][i] = ext->ctrl_bits[i];
        }
        if (ext->recolor == 1)
            j["btnm"]["recolor"] = true;
        else
            j["btnm"]["recolor"] = false;
        if (ext->one_toggle == 1)
            j["btnm"]["oneToggle"] = true;
        else
            j["btnm"]["oneToggle"] = false;
        for (int i = 0; i < _LV_BTN_STATE_NUM; i++)
        {
            lv_style_t* sty = (lv_style_t*)ext->styles_btn[i];
            j["btnm"]["btnStyles"][i] = Style::ToJSON(*sty);
        }

        return j;
    }

    lv_obj_t* LVButtonMatrix::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;

        lv_obj_t* btnm = lv_btnm_create(lv_scr_act(), nullptr);
        btnm = LVObject::FromJSON(j["base"], btnm);

        if (j["btnm"]["btnMap"].is_array())
        {
            std::vector<std::string> btnNames;
            int i = 0;
            while (j["btnm"]["btnMap"][i].is_string())
            {
                btnNames.push_back(j["btnm"]["btnMap"][i].get<std::string>());
                i++;
            }
            if (i > 0)
            {
                static std::vector<const char*> cstr;
                cstr.reserve(btnNames.size());
                for (size_t i = 0; i < btnNames.size(); ++i)
                    cstr.push_back(const_cast<char*>(btnNames[i].c_str()));
                cstr.push_back("");
                lv_btnm_set_map(btnm, &cstr[0]);
            }
        }
        std::stringstream ss;
        ss << j.dump();
        std::string s = ss.str();
        if (j["btnm"]["ctrl"].is_array())
        {
            int i = 0;
            static std::vector<lv_btnm_ctrl_t> ctrlMap;
            while (j["btnm"]["ctrl"][i].is_number())
            {
                ctrlMap.push_back(j["btnm"]["ctrl"][i].get<uint16_t>());
                i++;
            }


            lv_btnm_set_ctrl_map(btnm, &ctrlMap[0]);
        }
        if (j["btnm"]["recolor"].is_boolean())
        {
            if (j["btnm"]["recolor"].get<bool>())
                lv_btnm_set_recolor(btnm, true);
            else
                lv_btnm_set_recolor(btnm, false);
        }
        if (j["btnm"]["oneToggle"].is_boolean())
        {
            if (j["btnm"]["oneToggle"].get<bool>())
                lv_btnm_set_one_toggle(btnm, true);
            else
                lv_btnm_set_one_toggle(btnm, false);
        }
        if (j["btnm"]["btnStyles"].is_array())
        {
            int i = 0;
            while (j["btnm"]["btnStyles"][i].is_object())
            {
                switch (i)
                {
                case 0:
                    {
                        const lv_style_t style = Style::FromJSON(j["btnm"]["btnStyles"][i]);
                        lv_btnm_set_style(btnm, LV_BTNM_STYLE_BG, &style);
                        break;
                    }
                case 1:
                    {
                        const lv_style_t style = Style::FromJSON(j["btnm"]["btnStyles"][i]);
                        lv_btnm_set_style(btnm, LV_BTNM_STYLE_BTN_REL, &style);
                        break;
                    }
                case 2:
                    {
                        const lv_style_t style = Style::FromJSON(j["btnm"]["btnStyles"][i]);
                        lv_btnm_set_style(btnm, LV_BTNM_STYLE_BTN_PR, &style);
                        break;
                    }
                case 3:
                    {
                        const lv_style_t style = Style::FromJSON(j["btnm"]["btnStyles"][i]);
                        lv_btnm_set_style(btnm, LV_BTNM_STYLE_BTN_TGL_REL, &style);
                        break;
                    }
                case 4:
                    {
                        const lv_style_t style = Style::FromJSON(j["btnm"]["btnStyles"][i]);
                        lv_btnm_set_style(btnm, LV_BTNM_STYLE_BTN_TGL_PR, &style);
                        break;
                    }
                case 5:
                    {
                        const lv_style_t style = Style::FromJSON(j["btnm"]["btnStyles"][i]);
                        lv_btnm_set_style(btnm, LV_BTNM_STYLE_BTN_INA, &style);
                        break;
                    }
                }
                i++;
            }
        }
        return btnm;
    }

    bool LVButtonMatrix::SetValue(lv_obj_t* obj, std::string key, int value)
    {
        if (key == "/btnm/recolor")
        {
            if (value == 1)
                lv_btnm_set_recolor(obj, true);
            else
                lv_btnm_set_recolor(obj, false);
            return true;
        }
        if (key == "/btnm/oneToggle")
        {
            if (value == 1)
                lv_btnm_set_one_toggle(obj, true);
            else
                lv_btnm_set_one_toggle(obj, false);
            return true;
        }
        return false;
    }

    bool LVButtonMatrix::SetValue(lv_obj_t* obj, std::string key, lv_style_t* style)
    {
        if (key == "/btnm/btnStyles/0" || key == "/btnm/btnStyles/bg")
        {
            lv_btnm_set_style(obj, LV_BTNM_STYLE_BG, style);
            return true;
        }
        if (key == "/btnm/btnStyles/1" || key == "/btnm/btnStyles/btnRel")
        {
            lv_btnm_set_style(obj, LV_BTNM_STYLE_BTN_REL, style);
            return true;
        }
        if (key == "/btnm/btnStyles/2" || key == "/btnm/btnStyles/btnPr")
        {
            lv_btnm_set_style(obj, LV_BTNM_STYLE_BTN_PR, style);
            return true;
        }
        if (key == "/btnm/btnStyles/3" || key == "/btnm/btnStyles/tglRel")
        {
            lv_btnm_set_style(obj, LV_BTNM_STYLE_BTN_TGL_REL, style);
            return true;
        }
        if (key == "/btnm/btnStyles/4" || key == "/btnm/btnStyles/tglPr")
        {
            lv_btnm_set_style(obj, LV_BTNM_STYLE_BTN_TGL_PR, style);
            return true;
        }
        if (key == "/btnm/btnStyles/5" || key == "/btnm/btnStyles/ina")
        {
            lv_btnm_set_style(obj, LV_BTNM_STYLE_BTN_INA, style);
            return true;
        }
        return false;
    }

    bool LVButtonMatrix::SetValue(lv_obj_t* obj, std::string key, std::vector<std::string> value)
    {
        if (key == "/btnm/btnMap")
        {
            std::vector<const char*> btnNames;
            int i = 0;
            for (int i = 0; i < value.size(); i++)
                btnNames.push_back(value[i].c_str());
            lv_btnm_set_map(obj, &btnNames[0]);
            return true;
        }
        return false;
    }

    bool LVButtonMatrix::SetValue(lv_obj_t* obj, std::string key, std::vector<uint16_t> value)
    {
        if (key == "/btnm/ctrl")
        {
            lv_btnm_set_ctrl_map(obj, &value[0]);
            return true;
        }

        return false;
    }
}
#endif

#endif // selected widget
