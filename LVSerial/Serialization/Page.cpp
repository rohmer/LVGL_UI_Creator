#include "Page.h"
#include "ObjectUserData.h"
std::map<unsigned int, lv_obj_t*> Serialization::Page::idToObj;

namespace Serialization
{
    json Page::ToJSON(std::vector<lv_obj_t*> objects)
    {
        json page;

        // Normalize styles so we only have them defined once, and reused
        for (int i = 0; i < objects.size(); i++)
        {
            getStyles(objects[i]);
        }

        for (int i = 0; i < objects.size(); i++)
        {
            auto oud = static_cast<ObjectUserData*>(lv_obj_get_user_data(objects[i]));
            idToObj.emplace(oud->objectID, objects[i]);
            page["obj"][i] = serializeObject(objects[i]);
            page["obj"][i]["oid"] = oud->objectID;
            if (objects[i]->par != nullptr)
            {
                auto pid = static_cast<ObjectUserData*>(lv_obj_get_user_data(objects[i]->par));
                page["obj"][i]["par"] = pid->objectID;
            }
            else
            {
                page["obj"][i]["par"] = -1; // Parent is the screen
            }
        }
        
        return page;
    }

    void Page::getStyles(lv_obj_t* obj)
    {
        addStyle("base",lv_obj_get_style(obj));
        
        lv_obj_type_t type;
        lv_obj_get_type(obj, &type);

        if (strcmp(type.type[0], "lv_arc") == 0)
        {
            addStyle("arc", lv_arc_get_style(obj, LV_ARC_STYLE_MAIN));
        }
        if (strcmp(type.type[0], "lv_bar") == 0)
        {
            addStyle("arc", lv_arc_get_style(obj, LV_BAR_STYLE_BG));
            addStyle("arc", lv_arc_get_style(obj, LV_BAR_STYLE_INDIC));
        }
        if (strcmp(type.type[0], "lv_btn") == 0)
        {
            addStyle("btn", lv_btn_get_style(obj,LV_BTN_STATE_INA));
            addStyle("btn", lv_btn_get_style(obj, LV_BTN_STATE_PR));
            addStyle("btn", lv_btn_get_style(obj, LV_BTN_STATE_TGL_REL));
            addStyle("btn", lv_btn_get_style(obj, LV_BTN_STATE_TGL_PR));
            addStyle("btn", lv_btn_get_style(obj, LV_BTN_STATE_INA));
        }
    }

    void Page::addStyle(std::string styleName, const lv_style_t* style)
    {
        if (style == nullptr)
            return;
        if (styles.find((lv_style_t*)style) != styles.end())
        {
            int i = 0;
            std::stringstream ss;
            ss << styleName << "." << i;
            while (nameToStyle.find(ss.str()) != nameToStyle.end())
            {
                ss.str("");
                i++;
                ss << styleName << "." << i;
            }
            styles.emplace(style, ss.str());
            nameToStyle.emplace(ss.str(), style);
        }
    }
    json Page::serializeObject(lv_obj_t* object)
    {
        lv_obj_type_t* objType = new lv_obj_type_t();
        lv_obj_get_type(object, objType);
        if (strcmp(objType->type[0], "lv_arc") == 0)
        {
            return LVArc::ToJSON(object);
        }
        if (strcmp(objType->type[0], "lv_bar") == 0)
        {
            return LVBar::ToJSON(object);
        }
        if (strcmp(objType->type[0], "lv_btn") == 0)
        {
            return LVButton::ToJSON(object);
        }
        if (strcmp(objType->type[0], "lv_btnm") == 0)
        {
            return LVButtonMatrix::ToJSON(object);
        }
        if (strcmp(objType->type[0], "lv_cal") == 0)
        {
            return LVCalendar::ToJSON(object);
        }
        if (strcmp(objType->type[0], "lv_canvas") == 0)
        {
            return LVCanvas::ToJSON(object);
        }
        if (strcmp(objType->type[0], "lv_cb") == 0)
        {
            return LVCheckBox::ToJSON(object);
        }
    }

    std::vector<lv_obj_t*> Page::FromJSON(json j)
    {
        std::vector<lv_obj_t*> objects;
        std::map<std::string, lv_style_t> styles;

        return objects;
    }
}
