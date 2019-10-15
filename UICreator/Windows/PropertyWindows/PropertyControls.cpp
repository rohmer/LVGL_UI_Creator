#include "PropertyControls.h"

lv_obj_t* PropertyControls::createStyleEntry(PropertyWindow* pw, lv_obj_t* parent, std::string labelTxt,
                                             std::string propertyPath)
{
    lv_obj_t* styleCont = lv_cont_create(parent, nullptr);
    lv_cont_set_layout(styleCont, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(styleCont, LV_LAYOUT_GRID);
    lv_obj_set_style(styleCont, &lv_style_transp);

    lv_obj_t* styleLab = lv_label_create(styleCont, nullptr);
    lv_label_set_text(styleLab, labelTxt.c_str());
    lv_obj_t* styleDD = lv_ddlist_create(styleCont, nullptr);
    std::stringstream ss;
    for (auto it = pw->Styles.begin();
         it != pw->Styles.end();
         ++it)
    {
        ss << (*it).first << "\n";
    }
    lv_ddlist_set_options(styleDD, ss.str().c_str());
    lv_ddlist_set_draw_arrow(styleDD, true);
    sPropChange* pc1 = new sPropChange();
    pc1->pw = pw;
    pc1->propertyPath = propertyPath;
    lv_obj_set_user_data(styleDD, (lv_obj_user_data_t)pc1);
    lv_obj_set_event_cb(styleDD, PropertyControls::styleListCB);
    lv_obj_t* sLab = lv_label_create(styleCont, nullptr);
    lv_label_set_text(sLab, "New Style");
    lv_obj_t* newSBtn = lv_btn_create(styleCont, nullptr);
    lv_obj_t* sBtnImg = lv_label_create(newSBtn, nullptr);
    lv_obj_set_size(newSBtn, 35, 35);
    lv_label_set_text(sBtnImg, LV_SYMBOL_EDIT);
    lv_obj_set_user_data(newSBtn, (lv_obj_user_data_t)pw);
    lv_obj_set_event_cb(newSBtn, PropertyControls::createStyleCB);
    return styleDD;
}


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

lv_obj_t* PropertyControls::createCBEntry(
    PropertyWindow* pw,
    lv_obj_t* parent,
    std::string labelTxt,
    std::string propertyPath)
{
    lv_obj_t* cb = lv_cb_create(parent, nullptr);
    lv_cb_set_text(cb, labelTxt.c_str());
    sPropChange* propChange = new sPropChange();
    propChange->pw = pw;
    propChange->propertyPath = propertyPath;
    lv_obj_set_user_data(cb, (lv_obj_user_data_t)propChange);
    lv_obj_set_event_cb(cb, checkBoxCB);
    return cb;
}

lv_obj_t* PropertyControls::createNumericEntry(PropertyWindow* pw, lv_obj_t* parent, const std::string labelTxt,
                                               const std::string path)
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
        lv_group_t* kbGroup = pc->pw->GetKBGroup();
        lv_group_remove_all_objs(kbGroup);
        lv_group_add_obj(kbGroup, obj);
    }
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        if (pc->pw->GetSelectedObject() == nullptr)
            return;
        ObjectUserData* oud = (ObjectUserData*)lv_obj_get_user_data(pc->pw->GetSelectedObject());

        json j = oud->objectJson;
        j[nlohmann::json_pointer<std::string>(pc->propertyPath)] = atoi(lv_ta_get_text(obj));

        std::vector<std::string> propTokens = ObjectTools::Split(pc->propertyPath, '/');
        std::string oType = propTokens[1];
        if (oType == "base")
        {
            if (!Serialization::LVObject::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath,
                                                   atoi(lv_ta_get_text(obj))))
            {
                spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
            }
        }
        if (oType == "arc")
        {
            if (!Serialization::LVArc::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath,
                                                atoi(lv_ta_get_text(obj))))
            {
                spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
            }
        }
        if (oType == "bar")
        {
            if (!Serialization::LVBar::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath,
                                                atoi(lv_ta_get_text(obj))))
            {
                spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
            }
        }
        oud->objectJson = j;
        lv_obj_set_user_data(pc->pw->GetSelectedObject(), (lv_obj_user_data_t)oud);
    }
    if (event == LV_EVENT_VALUE_CHANGED)
    {
    }
}

void PropertyControls::checkBoxCB(lv_obj_t* obj, lv_event_t event)
{
    sPropChange* pc = (sPropChange*)lv_obj_get_user_data(obj);
    if (pc->pw->Drawing())
        return;
    if (event == LV_EVENT_CLICKED)
    {
        std::vector<std::string> propTokens = ObjectTools::Split(pc->propertyPath, '/');
        std::string oType = propTokens[1];
        if (oType == "base")
        {
            if (!Serialization::LVObject::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath, lv_cb_is_checked(obj))
            )
            {
                spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
            }
        }
        if (oType == "arc")
        {
            spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
        }
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

void PropertyControls::styleListCB(lv_obj_t* obj, lv_event_t event)
{
    sPropChange* pc = (sPropChange*)lv_obj_get_user_data(obj);
    if (pc->pw->Drawing())
        return;
    if (event == LV_EVENT_CLICKED)
    {
        lv_group_t* kbGroup = pc->pw->GetKBGroup();
        lv_group_remove_all_objs(kbGroup);
        lv_group_add_obj(kbGroup, obj);
    }
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        std::vector<std::string> propTokens = ObjectTools::Split(pc->propertyPath, '/');
        std::string oType = propTokens[1];
        if (oType == "base")
        {
            char val[32];
            lv_ddlist_get_selected_str(obj, val, 32);
            std::string value = val;
            Serialization::LVObject::SetStyle(pc->pw->GetSelectedObject(),
                                              pc->propertyPath,
                                              value,
                                              pc->pw->StylePtrs);
        }
        if (oType == "bar")
        {
            char val[32];
            lv_ddlist_get_selected_str(obj, val, 32);
            std::string value = val;
            Serialization::LVBar::SetValue(pc->pw->GetSelectedObject(),
                                           pc->propertyPath,
                                           pc->pw->StylePtrs[val]);
        }
    }
}

void PropertyControls::createStyleCB(lv_obj_t* obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        // TODO: Show create style window
    }
}
