#include "LVButtonMatrix.h"

namespace Serialization
{
    json LVButtonMatrix::ToJSON(lv_obj_t* btnm)
    {
        json j;
        j["base"] = LVObject::ToJSON(btnm);
        lv_btnm_ext_t* ext = static_cast<lv_btnm_ext_t*>(lv_obj_get_ext_attr(btnm));
        int btnCnt = ext->btn_cnt;
        int buttonCt = ext->btn_cnt;
        const char** ma = ext->map_p;
        int ctr = 0;
        int row = 0;
        while (*ma)
        {
            const char* p = *ma;
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
