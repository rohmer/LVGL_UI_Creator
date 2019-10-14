#include "PropertyControls.h"

void PropertyControls::assignColor(lv_color_t color, std::any objectData)
{
    PropertyWindow::sOData odata = std::any_cast<PropertyWindow::sOData>(objectData);
    if (odata.objName == "ArcColor")
    {
        static lv_style_t newStyle;
        lv_style_copy(&newStyle, &lv_style_plain);

        int lineWidth = std::atoi(lv_ta_get_text(ArcProperties::arcLineWidth));
        newStyle.line.width = lineWidth;
        color.ch.alpha = 255;
        newStyle.line.color = color;
        if (lv_cb_is_checked(ArcProperties::arcLineRound))
            newStyle.line.rounded = true;
        else
            newStyle.line.rounded = false;
        lv_arc_set_style(odata.pw->GetSelectedObject(), LV_ARC_STYLE_MAIN, &newStyle);
    }
}

lv_obj_t* PropertyControls::createNumericEntry(PropertyWindow *pw, lv_obj_t* parent, const std::string labelTxt, const std::string path)
{
    lv_obj_t* label = lv_label_create(parent, nullptr);
    lv_label_set_text(label, labelTxt.c_str());
    lv_obj_t* obj = lv_ta_create(parent, nullptr);
    lv_ta_set_one_line(obj, true);
    lv_ta_set_accepted_chars(obj, "0123456789");
    lv_ta_set_text(obj, "");
    lv_ta_set_cursor_type(obj, LV_CURSOR_NONE);
    sPropChange* propChange = new sPropChange();
    propChange->pw = pw;
    propChange->propertyPath = path;
    lv_obj_set_user_data(obj, (lv_obj_user_data_t)propChange);
    lv_obj_set_event_cb(obj, numericEntryCB);
    lv_obj_set_width(obj, 35);
    return obj;
}

void PropertyControls::numericEntryCB(lv_obj_t* obj, lv_event_t event)
{
    sPropChange* pc = (sPropChange*)lv_obj_get_user_data(obj);

    if (pc->pw->Drawing())
        return;
    if (event == LV_EVENT_CLICKED)
    {
        lv_ta_set_cursor_type(obj, LV_CURSOR_LINE);
    }
    if (event == LV_EVENT_DEFOCUSED)
    {
        lv_ta_set_cursor_type(obj, LV_CURSOR_NONE);
    }
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        ObjectUserData* oud = (ObjectUserData*)lv_obj_get_user_data(pc->pw->GetSelectedObject());
        json j = oud->objectJson;
        j[nlohmann::json_pointer<std::string>(pc->propertyPath)] = atoi(lv_ta_get_text(obj));

        std::vector<std::string> propTokens = ObjectTools::Split(pc->propertyPath, '/');
        std::string oType = propTokens[0];
        if (oType == "base")
        {
            if (!Serialization::LVObject::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath, atoi(lv_ta_get_text(obj))))
            {
                spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
            }
        }
        if (oType == "arc")
        {
            if (!Serialization::LVArc::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath, atoi(lv_ta_get_text(obj))))
            {
                spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
            }
        }

        oud->objectJson = j;
        lv_obj_set_user_data(obj, (lv_obj_user_data_t)oud);
    }
}

void PropertyControls::checkBoxCB(lv_obj_t* obj, lv_event_t event)
{
    sPropChange* pc = (sPropChange*)lv_obj_get_user_data(obj);
    if (pc->pw->Drawing())
        return;
    if (event == LV_EVENT_CLICKED)
    {
        // TODO: Set the value for the checkbox
    }
}

void PropertyControls::ddListCB(lv_obj_t* obj, lv_event_t event)
{
    sPropChange* pc = (sPropChange*)lv_obj_get_user_data(obj);
    if (pc->pw->Drawing())
        return;
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        // TODO: Set the value for the ddList
    }
}

void PropertyControls::createStyleCB(lv_obj_t* obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        // TODO: Show create style window
    }
}