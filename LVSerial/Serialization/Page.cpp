#include "Page.h"
#include "../../UICreator/ObjectUserData.h"
std::map<unsigned int, lv_obj_t*> Serialization::Page::idToObj;

namespace Serialization
{
    json Page::ToJSON(std::vector<lv_obj_t*> objects)
    {
        json page;
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
