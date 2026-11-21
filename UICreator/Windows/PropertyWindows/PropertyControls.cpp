#include "PropertyControls.h"
#include "../../Utils/EditorStyle.h"
#include <charconv>
#include "../../Utils/EditorControls.h"
#include "../../Utils/EditorEvents.h"

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
    if(propertyPath == "/base/style") ss << "Custom / current";
    for (auto it = pw->Styles.begin();
         it != pw->Styles.end();
         ++it)
    {
        if(it != pw->Styles.begin() || propertyPath == "/base/style") ss << "\n";
        ss << (*it).first;
    }
    lv_ddlist_set_options(styleDD, ss.str().c_str());
    lv_ddlist_set_draw_arrow(styleDD, true);
    sPropChange* pc1 = new sPropChange();
    pc1->pw = pw;
    pc1->propertyPath = propertyPath;
    lv_obj_set_user_data(styleDD, (lv_obj_user_data_t)pc1);
    SetEditorEventCallback<PropertyControls::styleListCB>(styleDD);
    lv_obj_t* sLab = lv_label_create(styleCont, nullptr);
    lv_label_set_text(sLab, "New Style");
    lv_obj_t* newSBtn = lv_btn_create(styleCont, nullptr);
    lv_obj_t* sBtnImg = lv_label_create(newSBtn, nullptr);
    lv_obj_set_size(newSBtn, 35, 35);
    lv_label_set_text(sBtnImg, LV_SYMBOL_EDIT);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_width(styleCont, LV_PCT(100));
    lv_obj_set_width(styleDD, LV_PCT(100));
    lv_obj_set_flex_flow(styleCont, LV_FLEX_FLOW_COLUMN);
    EditorStyle::Control(styleDD);
    lv_obj_set_hidden(sLab, true);
    lv_obj_set_hidden(newSBtn, true);
    lv_obj_center(sBtnImg);
#endif
    lv_obj_set_user_data(newSBtn, (lv_obj_user_data_t)pw);
    SetEditorEventCallback<PropertyControls::createStyleCB>(newSBtn);
    return styleDD;
}


void PropertyControls::assignColor(lv_color_t color, std::any objectData)
{
    PropertyWindow::sOData odata = std::any_cast<PropertyWindow::sOData>(objectData);
    if (odata.objName == "ArcColor")
    {
#if LVGL_VERSION_MAJOR >= 9
        Serialization::LVArc::SetValue(odata.pw->GetSelectedObject(), "/arc/style/line.color",
                                       lv_color_to_int(color));
#else
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
#endif
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
    SetEditorEventCallback<checkBoxCB>(cb);
    return cb;
}

lv_obj_t* PropertyControls::createNumericEntry(PropertyWindow* pw, lv_obj_t* parent, const std::string labelTxt,
                                               const std::string path)
{
#if LVGL_VERSION_MAJOR >= 9
    auto* field = lv_cont_create(parent, nullptr);
    lv_obj_set_size(field, 78, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(field, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(field, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(field, LV_OPA_TRANSP, LV_PART_MAIN);
    parent = field;
#endif
    lv_obj_t* label = lv_label_create(parent, nullptr);
    lv_label_set_text(label, labelTxt.c_str());
    lv_obj_t* obj = lv_ta_create(parent, nullptr);
    lv_ta_set_one_line(obj, true);
    lv_ta_set_accepted_chars(obj, "-0123456789");
    lv_ta_set_text(obj, "");
    lv_ta_set_cursor_type(obj, LV_CURSOR_NONE);
    sPropChange* propChange = new sPropChange();
    propChange->pw = pw;
    propChange->propertyPath = path;
    lv_obj_set_user_data(obj, (lv_obj_user_data_t)propChange);
    SetEditorEventCallback<numericEntryCB>(obj);
    lv_obj_set_width(obj, 35);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_size(obj, LV_PCT(100), 36);
    lv_obj_set_style_pad_all(obj, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 5, LV_PART_MAIN);
    EditorStyle::Control(obj);
    if(path.find("/coords/") != std::string::npos || path.find("width") != std::string::npos || path.find("Width") != std::string::npos) EditorStyle::Help(parent,"px");
    else if(path.find("/angle/") != std::string::npos) EditorStyle::Help(parent,"degrees");
    lv_obj_add_event_cb(obj,[](lv_event_t* event){delete static_cast<sPropChange*>(lv_event_get_user_data(event));},LV_EVENT_DELETE,propChange);
#endif
    return obj;
}

void PropertyControls::numericEntryCB(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED && event != LV_EVENT_VALUE_CHANGED) return;
    sPropChange* pc = (sPropChange*)lv_obj_get_user_data(obj);

    if (pc->pw->Drawing())
        return;
    if (event == LV_EVENT_CLICKED)
    {
        lv_ta_set_cursor_type(obj, LV_CURSOR_LINE);
        lv_group_t* kbGroup = pc->pw->GetKBGroup();
#if LVGL_VERSION_MAJOR >= 9
        FocusEditorControl(kbGroup, obj);
#else
        lv_group_remove_all_objs(kbGroup);
        lv_group_add_obj(kbGroup, obj);
#endif
    }
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        if (pc->pw->GetSelectedObject() == nullptr)
            return;
        ObjectUserData* oud = (ObjectUserData*)lv_obj_get_user_data(pc->pw->GetSelectedObject());

        const std::string input = lv_ta_get_text(obj);
        int parsedValue = 0;
        const auto parsed = std::from_chars(input.data(), input.data()+input.size(), parsedValue);
        const bool dimension = pc->propertyPath == "/base/coords/width" || pc->propertyPath == "/base/coords/height";
        const bool opacity = pc->propertyPath == "/base/opascale";
        if(parsed.ec != std::errc{} || parsed.ptr != input.data()+input.size() || (dimension && parsedValue < 1) || (opacity && (parsedValue < 0 || parsedValue > 255))) {
            lv_obj_set_style_border_color(obj, lv_color_hex(EditorStyle::Error), LV_PART_MAIN);
            return;
        }
        lv_obj_set_style_border_color(obj, lv_color_hex(EditorStyle::Border), LV_PART_MAIN);
        json j = oud->objectJson;
        j[nlohmann::json_pointer<std::string>(pc->propertyPath)] = parsedValue;

        std::vector<std::string> propTokens = ObjectTools::Split(pc->propertyPath, '/');
        std::string oType = propTokens[1];
        if (oType == "base")
        {
            if (!Serialization::LVObject::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath,
                                                   parsedValue))
            {
                spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
            }
        }
        if (oType == "arc")
        {
            if (!Serialization::LVArc::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath,
                                                parsedValue))
            {
                spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
            }
        }
        if (oType == "bar")
        {
            if (!Serialization::LVBar::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath,
                                                parsedValue))
            {
                spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
            }
        }
        if (oType == "button")
        {
            if (!Serialization::LVButton::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath,
                parsedValue))
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

void PropertyControls::checkBoxCB(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED) return;
    sPropChange* pc = (sPropChange*)lv_obj_get_user_data(obj);
    if (pc->pw->Drawing())
        return;
    if (event == LV_EVENT_CLICKED)
    {
#if LVGL_VERSION_MAJOR >= 9
        FocusEditorControl(pc->pw->GetKBGroup(), obj);
#endif
        if (!pc->pw->GetSelectedObject())
            return;
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
            Serialization::LVArc::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath, lv_cb_is_checked(obj));
        }
        if (oType == "bar")
            Serialization::LVBar::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath, lv_cb_is_checked(obj));
        if (oType == "button")
            Serialization::LVButton::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath, lv_cb_is_checked(obj));
        if (oType == "btnm")
            Serialization::LVButtonMatrix::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath, lv_cb_is_checked(obj));
    }
}

void PropertyControls::ddListCB(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_VALUE_CHANGED && event != LV_EVENT_CLICKED) return;
    sPropChange* pc = (sPropChange*)lv_obj_get_user_data(obj);
    if (pc->pw->Drawing())
        return;
#if LVGL_VERSION_MAJOR >= 9
    if (event == LV_EVENT_CLICKED)
    {
        FocusEditorControl(pc->pw->GetKBGroup(), obj);
        return;
    }
#endif
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        if (pc->pw->GetSelectedObject())
        {
            auto value = static_cast<int>(lv_ddlist_get_selected(obj));
            if (pc->propertyPath == "/base/dragDir")
                ++value;
            Serialization::LVObject::SetValue(pc->pw->GetSelectedObject(), pc->propertyPath, value);
        }
    }
}

void PropertyControls::styleListCB(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED && event != LV_EVENT_VALUE_CHANGED) return;
    sPropChange* pc = (sPropChange*)lv_obj_get_user_data(obj);
    if (pc->pw->Drawing())
        return;
    if (event == LV_EVENT_CLICKED)
    {
        lv_group_t* kbGroup = pc->pw->GetKBGroup();
#if LVGL_VERSION_MAJOR >= 9
        FocusEditorControl(kbGroup, obj);
#else
        lv_group_remove_all_objs(kbGroup);
        lv_group_add_obj(kbGroup, obj);
#endif
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
            if(value == "Custom / current") return;
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

void PropertyControls::createStyleCB(lv_obj_t* obj, EditorEventCode event)
{
    if (event == LV_EVENT_CLICKED)
    {
        // TODO: Show create style window
    }
}
