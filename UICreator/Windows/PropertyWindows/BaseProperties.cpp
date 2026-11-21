#include "BaseProperties.h"
#include "../../Utils/EditorStyle.h"
#include "../../Utils/EditorControls.h"
#include "../../Utils/EditorEvents.h"
lv_obj_t *BaseProperties::taX, *BaseProperties::taY, *BaseProperties::taWidth, *BaseProperties::taHeight;
lv_obj_t* BaseProperties::styleDD;
lv_obj_t *BaseProperties::hidden, *BaseProperties::click, *BaseProperties::top, *BaseProperties::parentEvent, *
         BaseProperties::opaScaleEnable, *BaseProperties::opaScale;
lv_obj_t *BaseProperties::drag, *BaseProperties::dragDir, *BaseProperties::dragThrow, *BaseProperties::dragParent;
lv_obj_t *BaseProperties::protNone, *BaseProperties::protPos, *BaseProperties::protFollow, *BaseProperties::protParent,
         *BaseProperties::protPressLost, *BaseProperties::protClickFocus, *BaseProperties::protChildChg;
CollapsableWindow* BaseProperties::baseObjProps;
#if LVGL_VERSION_MAJOR >= 9
namespace { lv_obj_t *objectNameEntry=nullptr, *positionLock=nullptr, *layoutHelp=nullptr, *resetPositionButton=nullptr; }
#endif

void BaseProperties::CreateBaseObjProps(PropertyWindow* propertyWin)
{
#if LVGL_VERSION_MAJOR >= 9
    auto section = [propertyWin](const char* title, bool collapsed=false) {
        auto* panel=new CollapsableWindow(propertyWin->GetWindow(),title,collapsed,10,0,380,0);
        auto* content=lv_cont_create(panel->GetWindow(),nullptr);
        lv_obj_set_size(content,LV_PCT(100),LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(content,LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_opa(content,LV_OPA_TRANSP,0);
        panel->AddObjectToWindow(content);
        propertyWin->GetCWM()->AddWindow(panel);
        return content;
    };
    auto* layout=section("Layout");
    auto* nameLabel=lv_label_create(layout);lv_label_set_text(nameLabel,"Object name");
    objectNameEntry=lv_textarea_create(layout);
    lv_textarea_set_one_line(objectNameEntry,true);lv_textarea_set_max_length(objectNameEntry,128);
    lv_obj_set_size(objectNameEntry,LV_PCT(100),EditorStyle::ControlHeight);EditorStyle::Control(objectNameEntry);
    lv_obj_add_event_cb(objectNameEntry,[](lv_event_t* event) {
        auto* pw=static_cast<PropertyWindow*>(lv_event_get_user_data(event));
        auto* control=lv_event_get_target_obj(event);
        if(lv_event_get_code(event)==LV_EVENT_CLICKED) FocusEditorControl(pw->GetKBGroup(),control);
        if(lv_event_get_code(event)==LV_EVENT_VALUE_CHANGED && !pw->Drawing()) {
            EditorEdit::Scope edit(control,true);
            const bool valid=pw->RenameSelected(lv_textarea_get_text(control));
            lv_obj_set_style_border_color(control,lv_color_hex(valid?EditorStyle::Border:EditorStyle::Error),LV_PART_MAIN);
        }
    },LV_EVENT_ALL,propertyWin);
    auto* position=lv_cont_create(layout,nullptr);lv_obj_set_size(position,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(position,LV_FLEX_FLOW_ROW_WRAP);
    taX=PropertyControls::createNumericEntry(propertyWin,position,"X","/base/coords/x");
    taY=PropertyControls::createNumericEntry(propertyWin,position,"Y","/base/coords/y");
    taWidth=PropertyControls::createNumericEntry(propertyWin,position,"Width","/base/coords/width");
    taHeight=PropertyControls::createNumericEntry(propertyWin,position,"Height","/base/coords/height");
    for(auto* control:{taX,taY,taWidth,taHeight}) lv_obj_set_width(lv_obj_get_parent(control),LV_PCT(46));
    layoutHelp=EditorStyle::Help(layout,"Coordinates are relative to the parent widget.");
    auto* reset=lv_button_create(layout);resetPositionButton=reset;EditorStyle::Button(reset);
    auto* resetLabel=lv_label_create(reset);lv_label_set_text(resetLabel,"Reset position");lv_obj_center(resetLabel);
    lv_obj_add_event_cb(reset,[](lv_event_t* event) {
        auto* pw=static_cast<PropertyWindow*>(lv_event_get_user_data(event));auto* object=pw->GetSelectedObject();
        if(!object || lv_obj_is_layout_positioned(object)) return;
        const auto* data=static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
        if(data && !data->objectJson.value("base",json::object()).value("drag",true)) return;
        EditorEdit::Scope edit;Serialization::LVObject::SetValue(object,"/base/coords/x",0);Serialization::LVObject::SetValue(object,"/base/coords/y",0);
        pw->SetSelectedObject(object,false,true);
    },LV_EVENT_CLICKED,propertyWin);
    auto* appearance=section("Appearance");
    styleDD=PropertyControls::createStyleEntry(propertyWin,appearance,"Style","/base/style");
    hidden=PropertyControls::createCBEntry(propertyWin,appearance,"Hidden on screen","/base/hidden");
    opaScale=PropertyControls::createNumericEntry(propertyWin,appearance,"Opacity (0-255)","/base/opascale");
    lv_obj_set_width(lv_obj_get_parent(opaScale),LV_PCT(100));
    auto* opacityReset=lv_button_create(appearance);EditorStyle::Button(opacityReset);
    auto* opacityLabel=lv_label_create(opacityReset);lv_label_set_text(opacityLabel,"Reset opacity");lv_obj_center(opacityLabel);
    lv_obj_add_event_cb(opacityReset,[](lv_event_t* event) {
        auto* pw=static_cast<PropertyWindow*>(lv_event_get_user_data(event));if(!pw->GetSelectedObject())return;
        EditorEdit::Scope edit;Serialization::LVObject::SetValue(pw->GetSelectedObject(),"/base/opascale",255);
        pw->Drawing(true);lv_textarea_set_text(opaScale,"255");pw->Drawing(false);
    },LV_EVENT_CLICKED,propertyWin);
    EditorStyle::Help(appearance,"Hidden widgets remain editable in Hierarchy. Font: choose Inherit / theme to restore the default.");
    auto* interaction=section("Interaction",true);
    positionLock=lv_checkbox_create(interaction);lv_checkbox_set_text(positionLock,"Lock position");
    lv_obj_add_event_cb(positionLock,[](lv_event_t* event) {
        auto* pw=static_cast<PropertyWindow*>(lv_event_get_user_data(event));
        FocusEditorControl(pw->GetKBGroup(),lv_event_get_target_obj(event));
    },LV_EVENT_CLICKED,propertyWin);
    lv_obj_add_event_cb(positionLock,[](lv_event_t* event) {
        auto* pw=static_cast<PropertyWindow*>(lv_event_get_user_data(event));auto* object=pw->GetSelectedObject();if(!object || pw->Drawing())return;
        EditorEdit::Scope edit;const bool locked=lv_obj_has_state(lv_event_get_target_obj(event),LV_STATE_CHECKED);
        Serialization::LVObject::SetValue(object,"/base/drag",!locked);
        BaseProperties::UpdateGlobalProps(pw,Serialization::ObjectSerializer::SerializeObject(object));
    },LV_EVENT_VALUE_CHANGED,propertyWin);
    EditorStyle::Help(interaction,"Locked widgets keep their position during canvas editing. Unlock before changing layout.");
    click=PropertyControls::createCBEntry(propertyWin,interaction,"Accept pointer input","/base/click");
    parentEvent=PropertyControls::createCBEntry(propertyWin,interaction,"Bubble events to parent","/base/parevent");
    auto* dirLabel=lv_label_create(interaction);lv_label_set_text(dirLabel,"Movement axis");
    dragDir=lv_dropdown_create(interaction);lv_dropdown_set_options(dragDir,"Horizontal\nVertical\nBoth");
    lv_obj_set_width(dragDir,LV_PCT(100));EditorStyle::Control(dragDir);
    auto* binding=new sPropChange();binding->pw=propertyWin;binding->propertyPath="/base/dragDir";
    lv_obj_set_user_data(dragDir,binding);SetEditorEventCallback<PropertyControls::ddListCB>(dragDir);
    propertyWin->GetCWM()->Update();
#else
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
    SetEditorEventCallback<PropertyControls::checkBoxCB>(hidden);

    click = lv_cb_create(attrCont, nullptr);
    lv_cb_set_text(click, "Click");
    sPropChange* pc3 = new sPropChange();
    pc3->pw = propertyWin;
    pc3->propertyPath = "/base/click";
    lv_obj_set_user_data(click, static_cast<lv_obj_user_data_t>(pc3));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(click);

    top = lv_cb_create(attrCont, nullptr);
    lv_cb_set_text(top, "Top");
    sPropChange* pc4 = new sPropChange();
    pc4->pw = propertyWin;
    pc4->propertyPath = "/base/top";

    lv_obj_set_user_data(top, static_cast<lv_obj_user_data_t>(pc4));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(top);

    parentEvent = lv_cb_create(attrCont, nullptr);
    lv_cb_set_text(parentEvent, "Parent event");
    sPropChange* pc5 = new sPropChange();
    pc5->pw = propertyWin;
    pc5->propertyPath = "/base/parevent";
    lv_obj_set_user_data(parentEvent, static_cast<lv_obj_user_data_t>(pc5));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(parentEvent);

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
    SetEditorEventCallback<PropertyControls::checkBoxCB>(opaScaleEnable);

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
    SetEditorEventCallback<PropertyControls::checkBoxCB>(drag);

    lv_obj_t* ddLab = lv_label_create(dragCont, nullptr);
    lv_label_set_text(ddLab, "   Dir");
    dragDir = lv_ddlist_create(dragCont, nullptr);
    lv_ddlist_set_options(dragDir, "HOR\nVERT\nALL");
    lv_ddlist_set_draw_arrow(dragDir, true);
    sPropChange* pc8 = new sPropChange();
    pc8->pw = propertyWin;
    pc8->propertyPath = "/base/dragDir";
    lv_obj_set_user_data(dragDir, static_cast<lv_obj_user_data_t>(pc8));
    SetEditorEventCallback<PropertyControls::ddListCB>(dragDir);

    dragThrow = lv_cb_create(dragCont, nullptr);
    lv_cb_set_text(dragThrow, "Throw");
    sPropChange* pc9 = new sPropChange();
    pc9->pw = propertyWin;
    pc9->propertyPath = "/base/dragThrow";

    lv_obj_set_user_data(dragThrow, static_cast<lv_obj_user_data_t>(pc9));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(dragThrow);

    dragParent = lv_cb_create(dragCont, nullptr);
    lv_cb_set_text(dragParent, "Parent");
    sPropChange* pc10 = new sPropChange();
    pc10->pw = propertyWin;
    pc10->propertyPath = "/base/dragParent";
    lv_obj_set_user_data(dragParent, static_cast<lv_obj_user_data_t>(pc10));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(dragParent);

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
    SetEditorEventCallback<PropertyControls::checkBoxCB>(protNone);
    protPos = lv_cb_create(row1, nullptr);
    lv_cb_set_text(protPos, "Position");
    sPropChange* pc12 = new sPropChange();
    pc12->pw = propertyWin;
    pc12->propertyPath = "/base/protect/protPos";
    lv_obj_set_user_data(protPos, static_cast<lv_obj_user_data_t>(pc12));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(protPos);
    protFollow = lv_cb_create(row1, nullptr);
    sPropChange* pc17 = new sPropChange();
    pc17->pw = propertyWin;
    pc17->propertyPath = "/base/protect/protFollow";
    lv_cb_set_text(protFollow, "Follow");
    lv_obj_set_user_data(protFollow, static_cast<lv_obj_user_data_t>(pc17));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(protFollow);
    protParent = lv_cb_create(row1, nullptr);
    lv_cb_set_text(protParent, "Parent");
    sPropChange* pc13 = new sPropChange();
    pc13->pw = propertyWin;
    pc13->propertyPath = "/base/protect/protParent";
    lv_obj_set_user_data(protParent, static_cast<lv_obj_user_data_t>(pc13));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(protParent);
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
    SetEditorEventCallback<PropertyControls::checkBoxCB>(protPressLost);
    protClickFocus = lv_cb_create(row2, nullptr);
    lv_cb_set_text(protClickFocus, "Click focus");
    sPropChange* pc15 = new sPropChange();
    pc15->pw = propertyWin;
    pc15->propertyPath = "/base/protect/protClickFocus";
    lv_obj_set_user_data(protClickFocus, static_cast<lv_obj_user_data_t>(pc15));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(protClickFocus);
    protChildChg = lv_cb_create(row2, nullptr);
    lv_cb_set_text(protChildChg, "Child change");
    sPropChange* pc16 = new sPropChange();
    pc16->pw = propertyWin;
    pc16->propertyPath = "/base/protect/protChildChange";
    lv_obj_set_user_data(protChildChg, static_cast<lv_obj_user_data_t>(pc16));
    SetEditorEventCallback<PropertyControls::checkBoxCB>(protChildChg);

#pragma endregion
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_style_bg_opa(boCont, LV_OPA_TRANSP, 0);
    lv_obj_set_width(posCont, LV_PCT(100));
    for(auto* control : {taX, taY, taWidth, taHeight})
        lv_obj_set_width(lv_obj_get_parent(control), LV_PCT(46));
    for(auto* row : {attrCont, attrCont2, dragCont, row1, row2}) {
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    }
    lv_obj_set_width(protCont, LV_PCT(100));
    lv_obj_set_height(protCont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(protCont, LV_OPA_TRANSP, 0);
#endif
    propertyWin->GetCWM()->AddWindow(baseObjProps);
#endif
}

void BaseProperties::UpdateGlobalProps(PropertyWindow* pw, json j)
{
#if LVGL_VERSION_MAJOR >= 9
    if(!pw->GetSelectedObject())return;
    pw->Drawing(true);
    const auto& base=j.at("base");
    lv_textarea_set_text(objectNameEntry,base.value("name",std::string{}).c_str());
    for(auto item:{std::make_pair(taX,"x"),std::make_pair(taY,"y"),std::make_pair(taWidth,"width"),std::make_pair(taHeight,"height")})
        lv_textarea_set_text(item.first,base.at("coords").at(item.second).dump().c_str());
    lv_obj_set_state(hidden,LV_STATE_CHECKED,base.value("hidden",false));
    lv_obj_set_state(click,LV_STATE_CHECKED,base.value("click",true));
    lv_obj_set_state(parentEvent,LV_STATE_CHECKED,base.value("parevent",false));
    const bool locked=!base.value("drag",true);
    lv_obj_set_state(positionLock,LV_STATE_CHECKED,locked);
    const bool managed=lv_obj_is_layout_positioned(pw->GetSelectedObject());
    for(auto* control:{taX,taY})lv_obj_set_state(control,LV_STATE_DISABLED,locked || managed);
    for(auto* control:{taWidth,taHeight})lv_obj_set_state(control,LV_STATE_DISABLED,locked);
    lv_obj_set_state(resetPositionButton,LV_STATE_DISABLED,locked || managed);
    lv_label_set_text(layoutHelp,managed?"The parent layout controls X and Y. Edit the parent layout or the widget size.":locked?"Position is locked. Unlock it in Interaction to edit layout.":"Coordinates are relative to the parent widget.");
    lv_textarea_set_text(opaScale,std::to_string(base.value("opascale",255)).c_str());
    lv_dropdown_set_selected(dragDir,std::clamp(base.value("dragDir",3)-1,0,2));
    lv_dropdown_set_selected(styleDD,0);
    int index=1;
    for(const auto& style:pw->Styles){if(style.first==base.value("style",json::object()).value("name",std::string{})){lv_dropdown_set_selected(styleDD,index);break;}++index;}
    pw->Drawing(false);
#else
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
    lv_ddlist_set_selected(dragDir, std::max(0, bj.value("dragDir", 3) - 1));
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
#endif
}
