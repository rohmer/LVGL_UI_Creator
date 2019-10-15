#include "BaseProperties.h"
lv_obj_t *BaseProperties::taX, *BaseProperties::taY, *BaseProperties::taWidth, *BaseProperties::taHeight;
lv_obj_t* BaseProperties::styleDD;
lv_obj_t *BaseProperties::hidden, *BaseProperties::click, *BaseProperties::top, *BaseProperties::parentEvent, *
         BaseProperties::opaScaleEnable, *BaseProperties::opaScale;
lv_obj_t *BaseProperties::drag, *BaseProperties::dragDir, *BaseProperties::dragThrow, *BaseProperties::dragParent;
lv_obj_t *BaseProperties::protNone, *BaseProperties::protPos, *BaseProperties::protFollow, *BaseProperties::protParent,
         *BaseProperties::protPressLost, *BaseProperties::protClickFocus, *BaseProperties::protChildChg;
CollapsableWindow* BaseProperties::baseObjProps;

void BaseProperties::CreateBaseObjProps(PropertyWindow* propertyWin)
{
    baseObjProps = new CollapsableWindow(propertyWin->GetWindow(), "Base Object", false, 10, 0, 380, 350);
    lv_obj_t* boCont = lv_cont_create(baseObjProps->GetWindow(), nullptr);
    lv_cont_set_layout(boCont, LV_LAYOUT_COL_L);
    lv_cont_set_fit(boCont, LV_FIT_FILL);

#pragma region Position/Area
    lv_obj_t* posCont = lv_cont_create(boCont, nullptr);
    lv_cont_set_layout(posCont, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(posCont, LV_LAYOUT_GRID);
    lv_obj_set_style(posCont, &lv_style_transp);

    taX = PropertyControls::createNumericEntry(propertyWin, posCont, "X", "/base/coords/x");
    taY = PropertyControls::createNumericEntry(propertyWin, posCont, "Y", "/base/coords/y");
    taWidth = PropertyControls::createNumericEntry(propertyWin, posCont, "Width", "/base/coords/width");
    taHeight = PropertyControls::createNumericEntry(propertyWin, posCont, "Height", "/base/coords/height");
#pragma endregion

    styleDD = PropertyControls::createStyleEntry(propertyWin, boCont, "Style", "/base/style");

#pragma region Attributes
    lv_obj_t* attrCont = lv_cont_create(boCont, nullptr);
    lv_cont_set_layout(attrCont, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(attrCont, LV_FIT_TIGHT);
    lv_obj_set_style(attrCont, &lv_style_transp);

    hidden = lv_cb_create(attrCont, nullptr);
    lv_cb_set_text(hidden, "Hidden");
    sPropChange* pc2 = new sPropChange();
    pc2->pw = propertyWin;
    pc2->propertyPath = "/base/hidden";
    lv_obj_set_user_data(hidden, static_cast<lv_obj_user_data_t>(pc2));
    lv_obj_set_event_cb(hidden, PropertyControls::checkBoxCB);

    click = lv_cb_create(attrCont, nullptr);
    lv_cb_set_text(click, "Click");
    sPropChange* pc3 = new sPropChange();
    pc3->pw = propertyWin;
    pc3->propertyPath = "/base/click";
    lv_obj_set_user_data(click, static_cast<lv_obj_user_data_t>(pc3));
    lv_obj_set_event_cb(click, PropertyControls::checkBoxCB);

    top = lv_cb_create(attrCont, nullptr);
    lv_cb_set_text(top, "Top");
    sPropChange* pc4 = new sPropChange();
    pc4->pw = propertyWin;
    pc4->propertyPath = "/base/top";

    lv_obj_set_user_data(top, static_cast<lv_obj_user_data_t>(pc4));
    lv_obj_set_event_cb(top, PropertyControls::checkBoxCB);

    parentEvent = lv_cb_create(attrCont, nullptr);
    lv_cb_set_text(parentEvent, "Parent event");
    sPropChange* pc5 = new sPropChange();
    pc5->pw = propertyWin;
    pc5->propertyPath = "/base/parevent";
    lv_obj_set_user_data(parentEvent, static_cast<lv_obj_user_data_t>(pc5));
    lv_obj_set_event_cb(parentEvent, PropertyControls::checkBoxCB);

    lv_obj_t* attrCont2 = lv_cont_create(boCont, nullptr);
    lv_cont_set_layout(attrCont2, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(attrCont2, LV_FIT_TIGHT);
    lv_obj_set_style(attrCont2, &lv_style_transp);

    opaScaleEnable = lv_cb_create(attrCont2, nullptr);
    lv_cb_set_text(opaScaleEnable, "Opa Scale Enable");
    sPropChange* pc6 = new sPropChange();
    pc6->pw = propertyWin;
    pc6->propertyPath = "/base/opascaleen";

    lv_obj_set_user_data(opaScaleEnable, static_cast<lv_obj_user_data_t>(pc6));
    lv_obj_set_event_cb(opaScaleEnable, PropertyControls::checkBoxCB);

    opaScale = PropertyControls::createNumericEntry(propertyWin, attrCont2, "Opa Scale", "/base/opascale");

#pragma endregion

#pragma region Drag
    lv_obj_t* dragCont = lv_cont_create(boCont, nullptr);
    lv_cont_set_layout(dragCont, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(dragCont, LV_LAYOUT_GRID);
    lv_obj_set_style(dragCont, &lv_style_transp);

    drag = lv_cb_create(dragCont, nullptr);
    lv_cb_set_text(drag, "Drag");
    sPropChange* pc7 = new sPropChange();
    pc7->pw = propertyWin;
    pc7->propertyPath = "/base/drag";
    lv_obj_set_user_data(drag, static_cast<lv_obj_user_data_t>(pc7));
    lv_obj_set_event_cb(drag, PropertyControls::checkBoxCB);

    lv_obj_t* ddLab = lv_label_create(dragCont, nullptr);
    lv_label_set_text(ddLab, "   Dir");
    dragDir = lv_ddlist_create(dragCont, nullptr);
    lv_ddlist_set_options(dragDir, "HOR\nVERT\nALL");
    lv_ddlist_set_draw_arrow(dragDir, true);
    sPropChange* pc8 = new sPropChange();
    pc8->pw = propertyWin;
    pc8->propertyPath = "/base/dragDir";
    lv_obj_set_user_data(dragDir, static_cast<lv_obj_user_data_t>(pc8));
    lv_obj_set_event_cb(dragDir, PropertyControls::ddListCB);

    dragThrow = lv_cb_create(dragCont, nullptr);
    lv_cb_set_text(dragThrow, "Throw");
    sPropChange* pc9 = new sPropChange();
    pc9->pw = propertyWin;
    pc9->propertyPath = "/base/dragThrow";

    lv_obj_set_user_data(dragThrow, static_cast<lv_obj_user_data_t>(pc9));
    lv_obj_set_event_cb(dragThrow, PropertyControls::checkBoxCB);

    dragParent = lv_cb_create(dragCont, nullptr);
    lv_cb_set_text(dragParent, "Parent");
    sPropChange* pc10 = new sPropChange();
    pc10->pw = propertyWin;
    pc10->propertyPath = "/base/dragParent";
    lv_obj_set_user_data(dragParent, static_cast<lv_obj_user_data_t>(pc10));
    lv_obj_set_event_cb(dragParent, PropertyControls::checkBoxCB);

#pragma endregion

#pragma region Protect
    lv_obj_t* protCont = lv_cont_create(boCont, nullptr);
    lv_obj_set_size(protCont, 360, 90);
    lv_cont_set_layout(protCont, LV_LAYOUT_COL_L);
    lv_cont_set_fit(protCont, LV_FIT_NONE);

    lv_obj_t* protLab = lv_label_create(protCont, nullptr);
    lv_label_set_text(protLab, "Protection");

    lv_obj_t* row1 = lv_cont_create(protCont, nullptr);
    lv_obj_set_style(row1, &lv_style_transp);
    lv_cont_set_layout(row1, LV_LAYOUT_ROW_T);
    lv_cont_set_fit(row1, LV_FIT_TIGHT);
    protNone = lv_cb_create(row1, nullptr);
    lv_cb_set_text(protNone, "None");
    sPropChange* pc11 = new sPropChange();
    pc11->pw = propertyWin;
    pc11->propertyPath = "/base/protect/protNone";
    lv_obj_set_user_data(protNone, static_cast<lv_obj_user_data_t>(pc11));
    lv_obj_set_event_cb(protNone, PropertyControls::checkBoxCB);
    protPos = lv_cb_create(row1, nullptr);
    lv_cb_set_text(protPos, "Position");
    sPropChange* pc12 = new sPropChange();
    pc12->pw = propertyWin;
    pc12->propertyPath = "/base/protect/protPos";
    lv_obj_set_user_data(protPos, static_cast<lv_obj_user_data_t>(pc12));
    lv_obj_set_event_cb(protPos, PropertyControls::checkBoxCB);
    protFollow = lv_cb_create(row1, nullptr);
    sPropChange* pc17 = new sPropChange();
    pc17->pw = propertyWin;
    pc17->propertyPath = "/base/protect/protFollow";
    lv_cb_set_text(protFollow, "Follow");
    lv_obj_set_user_data(protFollow, static_cast<lv_obj_user_data_t>(pc17));
    lv_obj_set_event_cb(protFollow, PropertyControls::checkBoxCB);
    protParent = lv_cb_create(row1, nullptr);
    lv_cb_set_text(protParent, "Parent");
    sPropChange* pc13 = new sPropChange();
    pc13->pw = propertyWin;
    pc13->propertyPath = "/base/protect/protParent";
    lv_obj_set_user_data(protParent, static_cast<lv_obj_user_data_t>(pc13));
    lv_obj_set_event_cb(protParent, PropertyControls::checkBoxCB);
    lv_obj_t* row2 = lv_cont_create(protCont, nullptr);
    lv_obj_set_style(row2, &lv_style_transp);
    lv_cont_set_layout(row2, LV_LAYOUT_ROW_T);
    lv_cont_set_fit(row2, LV_FIT_TIGHT);
    protPressLost = lv_cb_create(row2, nullptr);
    lv_cb_set_text(protPressLost, "Press lost");
    sPropChange* pc14 = new sPropChange();
    pc14->pw = propertyWin;
    pc14->propertyPath = "/base/protect/protPressLost";

    lv_obj_set_user_data(protPressLost, static_cast<lv_obj_user_data_t>(pc14));
    lv_obj_set_event_cb(protPressLost, PropertyControls::checkBoxCB);
    protClickFocus = lv_cb_create(row2, nullptr);
    lv_cb_set_text(protClickFocus, "Click focus");
    sPropChange* pc15 = new sPropChange();
    pc15->pw = propertyWin;
    pc15->propertyPath = "/base/protect/protClickFocus";
    lv_obj_set_user_data(protClickFocus, static_cast<lv_obj_user_data_t>(pc15));
    lv_obj_set_event_cb(protClickFocus, PropertyControls::checkBoxCB);
    protChildChg = lv_cb_create(row2, nullptr);
    lv_cb_set_text(protChildChg, "Child change");
    sPropChange* pc16 = new sPropChange();
    pc16->pw = propertyWin;
    pc16->propertyPath = "/base/protect/protChildChange";
    lv_obj_set_user_data(protChildChg, static_cast<lv_obj_user_data_t>(pc16));
    lv_obj_set_event_cb(protChildChg, PropertyControls::checkBoxCB);

#pragma endregion
    propertyWin->GetCWM()->AddWindow(baseObjProps);
}

void BaseProperties::UpdateGlobalProps(PropertyWindow* pw, json j)
{
    if (pw->GetSelectedObject() == nullptr)
        return;
    pw->Drawing(true);
    std::stringstream ss;
    std::stringstream x, y;
    json bj = j["base"];
    std::stringstream ss1;
    ss1 << j.dump();
    std::string s = ss1.str();
    x << bj["coords"]["x"];
    y << bj["coords"]["y"];
    lv_ta_set_text(taX, x.str().c_str());
    lv_ta_set_text(taY, y.str().c_str());
    int width = bj["coords"]["width"].get<int>();
    int height = bj["coords"]["height"].get<int>();
    std::stringstream ws, hs;
    ws << width;
    hs << height;
    lv_ta_set_text(taWidth, ws.str().c_str());
    lv_ta_set_text(taHeight, hs.str().c_str());

    std::string styleName = bj["style"]["name"];
    int selIdx = -1;
    int i = 0;
    for (std::map<std::string, json>::iterator it = pw->Styles.begin();
         it != pw->Styles.end();
         ++it)
    {
        if (it->first == styleName)
        {
            selIdx = i;
            break;
        }
        i++;
    }
    if (selIdx != -1)
        lv_ddlist_set_selected(styleDD, selIdx);
    lv_cb_set_checked(drag, bj["drag"]);
    lv_ddlist_set_selected(dragDir, bj["dragDir"]);
    lv_cb_set_checked(dragThrow, bj["dragThrow"]);

    lv_cb_set_checked(hidden, bj["hidden"]);
    lv_cb_set_checked(click, bj["click"]);
    lv_cb_set_checked(top, bj["top"]);
    lv_cb_set_checked(parentEvent, bj["parevent"]);
    lv_cb_set_checked(opaScaleEnable, bj["opascaleen"]);

    std::stringstream oss;
    oss << bj["opascale"];
    lv_ta_set_text(opaScale, oss.str().c_str());
    if (bj["protect"]["protNone"])
    {
        lv_cb_set_checked(protNone, true);
        lv_cb_set_checked(protPos, false);
        lv_cb_set_checked(protFollow, false);
        lv_cb_set_checked(protParent, false);
        lv_cb_set_checked(protPressLost, false);
        lv_cb_set_checked(protClickFocus, false);
        lv_cb_set_checked(protChildChg, false);
    }
    if (bj["protect"]["protChildChange"])
        lv_cb_set_checked(protChildChg, true);
    else
        lv_cb_set_checked(protChildChg, false);
    if (bj["protect"]["protParent"])
        lv_cb_set_checked(protParent, true);
    else
        lv_cb_set_checked(protParent, false);
    if (bj["protect"]["protPos"])
        lv_cb_set_checked(protPos, true);
    else
        lv_cb_set_checked(protPos, false);
    if (bj["protect"]["protFollow"])
        lv_cb_set_checked(protFollow, true);
    else
        lv_cb_set_checked(protFollow, false);
    if (bj["protect"]["protPressLost"])
        lv_cb_set_checked(protPressLost, true);
    else
        lv_cb_set_checked(protPressLost, false);
    if (bj["protect"]["protClickFocus"])
        lv_cb_set_checked(protClickFocus, true);
    else
        lv_cb_set_checked(protClickFocus, false);
    pw->Drawing(false);
}
