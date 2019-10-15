#include "ToolBar.h"

ToolBar::ToolBar(PropertyWindow* propertyWindow, SimWindow* simWindow) :
    propertyWindow(propertyWindow),
    simWindow(simWindow)
{
    int scWidth = lv_obj_get_width_fit(lv_scr_act());
    int x1 = scWidth / 4 + 5;
    int scHeight = lv_obj_get_height_fit(lv_scr_act());
    int height = scHeight / 3;
    int y1 = lv_obj_get_height_fit(lv_scr_act()) - height - 5;
    int width = scWidth - 400 - x1 - 5;
    window = new MinimizableWindow("Tools",
                                   x1,
                                   y1,
                                   width,
                                   height,
                                   nullptr,
                                   false,
                                   scWidth / 2 - 24,
                                   scHeight - 60);
    createObjects();
}

void ToolBar::createObjects()
{
    tabView = lv_tabview_create(window->GetWindowObj(), nullptr);
    lv_tabview_set_btns_pos(tabView, LV_TABVIEW_BTNS_POS_LEFT);
    contextTab = lv_tabview_add_tab(tabView, "Object\nTools");
    variableTab = lv_tabview_add_tab(tabView, "Variables");
    createObjectTools();
    createVariableTools();
}

void ToolBar::createObjectTools()
{
}

void ToolBar::createVariableTools()
{
    lv_cont_set_fit(variableTab, LV_FIT_NONE);
    lv_cont_set_layout(variableTab, LV_LAYOUT_OFF);
    lv_obj_t* varListContainer = lv_cont_create(variableTab, nullptr);
    lv_page_set_sb_mode(variableTab, LV_SB_MODE_OFF);
    lv_cont_set_fit(varListContainer, LV_FIT_TIGHT);
    lv_cont_set_layout(varListContainer, LV_LAYOUT_COL_L);
    lv_cont_set_style(varListContainer, LV_CONT_STYLE_MAIN, &lv_style_transp);
    lv_obj_t* vLab = lv_label_create(varListContainer, nullptr);
    lv_label_set_text(vLab, "Variables");
    variableList = lv_list_create(varListContainer, nullptr);
    lv_obj_set_pos(variableList, 5, 5);
    lv_obj_set_size(variableList, 125, lv_obj_get_height_fit(variableTab) - 35);
    lv_obj_t* btnCont = lv_cont_create(variableTab, nullptr);
    lv_obj_set_pos(btnCont, 135, 35);
    lv_cont_set_fit(btnCont, LV_FIT_TIGHT);
    lv_cont_set_layout(btnCont, LV_LAYOUT_COL_L);
    lv_cont_set_style(btnCont, LV_CONT_STYLE_MAIN, &lv_style_transp);
    addVarBtn = lv_btn_create(btnCont, nullptr);
    lv_obj_set_size(addVarBtn, 25, 35);
    lv_obj_t* lbl1 = lv_label_create(addVarBtn, nullptr);
    lv_label_set_text(lbl1, LV_SYMBOL_PLUS);
    lv_obj_set_event_cb(addVarBtn, addVarCallback);

    removeVarBtn = lv_btn_create(btnCont, nullptr);
    lv_obj_set_size(removeVarBtn, 25, 35);
    lv_obj_t* lbl2 = lv_label_create(removeVarBtn, nullptr);
    lv_label_set_text(lbl2, LV_SYMBOL_MINUS);
    lv_obj_set_event_cb(removeVarBtn, removeVarCallback);

    vPropCont = lv_cont_create(variableTab, nullptr);
    lv_obj_set_style(vPropCont, &lv_style_transp);
    lv_obj_set_pos(vPropCont, 175, 5);
    lv_obj_set_size(vPropCont, 225, lv_obj_get_height_fit(variableTab) - 10);

    lv_obj_t* lbl6 = lv_label_create(vPropCont, nullptr);
    lv_label_set_text(lbl6, "Type");
    lv_obj_set_pos(lbl6, 14, 15);

    lv_obj_t* lbl3 = lv_label_create(vPropCont, nullptr);
    lv_label_set_text(lbl3, "Name");
    lv_obj_set_pos(lbl3, 10, 55);
    varNameTA = lv_ta_create(vPropCont, nullptr);
    lv_ta_set_cursor_type(varNameTA, LV_CURSOR_NONE);
    lv_obj_set_size(varNameTA, 75, 15);
    lv_obj_set_pos(varNameTA, 50, 50);
    lv_ta_set_one_line(varNameTA, true);
    lv_ta_set_sb_mode(varNameTA, LV_SB_MODE_OFF);
    lv_ta_set_text(varNameTA, "");

    lv_obj_t* lbl4 = lv_label_create(vPropCont, nullptr);
    lv_label_set_text(lbl4, "History");
    lv_obj_set_pos(lbl4, 5, 95);
    varHistTA = lv_ta_create(vPropCont, nullptr);
    lv_ta_set_cursor_type(varHistTA, LV_CURSOR_NONE);
    lv_obj_set_size(varHistTA, 75, 15);
    lv_obj_set_pos(varHistTA, 50, 90);
    lv_ta_set_one_line(varHistTA, true);
    lv_ta_set_sb_mode(varHistTA, LV_SB_MODE_OFF);
    lv_ta_set_accepted_chars(varHistTA, "0123456789");
    lv_ta_set_text(varHistTA, "1");

    lv_obj_t* lbl5 = lv_label_create(vPropCont, nullptr);
    lv_label_set_text(lbl5, "Default");
    lv_obj_set_pos(lbl5, 5, 135);
    varDefaultValTA = lv_ta_create(vPropCont, nullptr);
    lv_ta_set_cursor_type(varDefaultValTA, LV_CURSOR_NONE);
    lv_obj_set_size(varDefaultValTA, 75, 15);
    lv_obj_set_pos(varDefaultValTA, 50, 130);
    lv_ta_set_one_line(varDefaultValTA, true);
    lv_ta_set_sb_mode(varDefaultValTA, LV_SB_MODE_OFF);
    lv_ta_set_text(varDefaultValTA, "");

    varDTDropDown = lv_ddlist_create(vPropCont, nullptr);
    lv_ddlist_set_draw_arrow(varDTDropDown, true);
    lv_ddlist_set_options(varDTDropDown, "String\nInteger\nBoolean\nFloat\nDateTime");
    lv_ddlist_set_selected(varDTDropDown, 1);
    lv_obj_set_pos(varDTDropDown, 50, 15);
    lv_ddlist_set_fix_width(varDTDropDown, 75);

    lv_obj_t* selPropBtn = lv_btn_create(vPropCont, nullptr);
    lv_obj_t* selPropLabel = lv_label_create(selPropBtn, nullptr);
    lv_label_set_text(selPropLabel, "Property");
    lv_obj_set_size(selPropBtn, 60, 80);
    lv_obj_set_pos(selPropBtn, 150, 5);

    lv_obj_set_style(lbl1, &lv_style_pretty_color);
    lv_obj_set_style(lbl2, &lv_style_pretty_color);
    lv_obj_set_style(lbl3, &lv_style_pretty_color);
    lv_obj_set_style(lbl4, &lv_style_pretty_color);
    lv_obj_set_style(lbl5, &lv_style_pretty_color);
    lv_obj_set_style(lbl6, &lv_style_pretty_color);
    lv_obj_set_style(variableList, &lv_style_pretty_color);
    lv_obj_set_style(varNameTA, &lv_style_pretty_color);
    lv_obj_set_style(varHistTA, &lv_style_pretty_color);
    lv_obj_set_style(varDefaultValTA, &lv_style_pretty_color);
    lv_obj_set_style(varDTDropDown, &lv_style_pretty_color);
    lv_obj_set_hidden(addVarBtn, true);

    lv_obj_set_user_data(varNameTA, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_event_cb(varNameTA, updateVariable);
    lv_obj_set_user_data(varHistTA, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_event_cb(varHistTA, updateVariable);
    lv_obj_set_user_data(varDefaultValTA, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_event_cb(varDefaultValTA, updateVariable);
    lv_obj_set_user_data(varDTDropDown, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_event_cb(varDTDropDown, updateVariable);
}

void ToolBar::SetContextActive()
{
    lv_tabview_set_tab_act(tabView, 0, true);
}

void ToolBar::SetSelectedObject()
{
    selectedObject = propertyWindow->GetSelectedObject();
}

// Callbacks

void ToolBar::selectProperty(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
}

void ToolBar::addVarCallback(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
}

void ToolBar::removeVarCallback(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
}

void ToolBar::updateVariable(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_VALUE_CHANGED)
        return;
}
