#include "ToolBar.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorEvents.h"
#include "../Utils/EditorStyle.h"
#include "../Utils/EditorIcons.h"
#include "../Utils/ObjectIcons.h"
#include <Serialization/ProjectScreens.h>
#include <Serialization/ObjectSerializer.h>
#include <SDL3/SDL.h>
#include <set>
#include <sstream>
#include <iomanip>
#include "../../CodeGeneration/CCodeGenerator.h"

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
#if LVGL_VERSION_MAJOR >= 9
    x1 = 528;
    width = scWidth - 400 - x1 - 5;
    height = std::max(height, 300);
    y1 = scHeight - height - 41;
#endif
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
    tabView = lv_tabview_create(EditorWindow::Content(window->GetWindowObj()), nullptr);
    lv_tabview_set_btns_pos(tabView, LV_TABVIEW_BTNS_POS_LEFT);
    contextTab = lv_tabview_add_tab(tabView, "Assets");
    variableTab = lv_tabview_add_tab(tabView, "Variables");
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_size(tabView, LV_PCT(100), LV_PCT(100));
    lv_tabview_set_tab_bar_position(tabView, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabView, 30);
#endif
    createObjectTools();
    createVariableTools();
}

void ToolBar::createObjectTools()
{
}

void ToolBar::createVariableTools()
{
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_style_pad_all(variableTab, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(variableTab, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(variableTab, LV_FLEX_FLOW_ROW);
    lv_obj_set_scrollable(variableTab, false);

    auto* sidebar = lv_cont_create(variableTab, nullptr);
    lv_obj_set_size(sidebar, LV_PCT(26), LV_PCT(100));
    lv_obj_set_style_min_width(sidebar, 112, LV_PART_MAIN);
    lv_obj_set_style_max_width(sidebar, 180, LV_PART_MAIN);
    lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(sidebar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sidebar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(sidebar, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(sidebar, false);
    auto* title = lv_label_create(sidebar);
    lv_label_set_text(title, "Variables");
    variableList = lv_list_create(sidebar, nullptr);
    lv_obj_set_width(variableList, LV_PCT(100));
    lv_obj_set_flex_grow(variableList, 1);
    lv_obj_set_style_pad_all(variableList, 6, LV_PART_MAIN);
    auto* buttons = lv_cont_create(sidebar, nullptr);
    lv_obj_set_size(buttons, LV_PCT(100), 36);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(buttons, 0, 0);
    lv_obj_set_style_border_width(buttons, 0, 0);
    lv_obj_set_scrollable(buttons, false);
    addVarBtn = lv_button_create(buttons);
    removeVarBtn = lv_button_create(buttons);
    for (auto* button : {addVarBtn, removeVarBtn})
    {
        lv_obj_set_size(button, 0, 34);
        lv_obj_set_flex_grow(button, 1);
        lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(button, 5, LV_PART_MAIN);
        auto* label = lv_label_create(button);
        lv_label_set_text(label, button == addVarBtn ? LV_SYMBOL_PLUS : LV_SYMBOL_MINUS);
        lv_obj_center(label);
        EditorIcons::Set(button, button == addVarBtn ? "variable-add" : "variable-delete", true, 20);
        lv_obj_set_user_data(button, this);
    }
    SetEditorEventCallback<addVarCallback>(addVarBtn);
    SetEditorEventCallback<removeVarCallback>(removeVarBtn);

    auto* variableDetails = lv_cont_create(variableTab, nullptr);
    lv_obj_set_size(variableDetails, 0, LV_PCT(100));
    lv_obj_set_flex_grow(variableDetails, 1);
    lv_obj_set_flex_flow(variableDetails, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(variableDetails, 8, 0);
    lv_obj_set_style_bg_opa(variableDetails, LV_OPA_TRANSP, 0);
    vPropCont = lv_cont_create(variableDetails, nullptr);
    lv_obj_set_width(vPropCont, LV_PCT(100));
    lv_obj_set_flex_grow(vPropCont, 1);
    lv_obj_set_height(vPropCont, 0);
    lv_obj_set_scrollable(vPropCont, true);
    lv_obj_set_scroll_dir(vPropCont, LV_DIR_VER);
    lv_obj_set_flex_flow(vPropCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(vPropCont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(vPropCont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(vPropCont, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(vPropCont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(vPropCont, 8, LV_PART_MAIN);
    auto makeRow = [this](const char* name) {
        auto* row = lv_cont_create(vPropCont, nullptr);
        lv_obj_set_size(row, LV_PCT(100), 38);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_scrollable(row, false);
        auto* label = lv_label_create(row);
        lv_obj_set_width(label, 96);
        lv_label_set_text(label, name);
        return row;
    };
    auto* typeRow = makeRow("Type");
    varDTDropDown = lv_dropdown_create(typeRow);
    lv_dropdown_set_options(varDTDropDown, "String\nInteger\nBoolean\nFloat\nDateTime");
    lv_dropdown_set_selected(varDTDropDown, 1);
    lv_obj_set_flex_grow(varDTDropDown, 1);
    lv_obj_set_width(varDTDropDown, 0);
    EditorStyle::Control(varDTDropDown);
    auto makeText = [&makeRow](const char* label, const char* value) {
        auto* field = lv_textarea_create(makeRow(label));
        lv_textarea_set_one_line(field, true);
        lv_textarea_set_text(field, value);
        lv_obj_set_flex_grow(field, 1);
        lv_obj_set_width(field, 0);
        lv_obj_set_height(field, 36);
        lv_obj_set_style_pad_all(field, 8, LV_PART_MAIN);
        lv_obj_set_style_radius(field, 5, LV_PART_MAIN);
        EditorStyle::Control(field);
        return field;
    };
    varNameTA = makeText("Name", "");
    varHistTA = makeText("History", "1");
    varDefaultValTA = makeText("Default", "");
    lv_textarea_set_accepted_chars(varHistTA, "0123456789");
    lv_textarea_set_max_length(varHistTA, 5);
    lv_textarea_set_placeholder_text(varNameTA, "e.g. temperature");
    lv_textarea_set_placeholder_text(varDefaultValTA, "Initial value for preview and generated UI");
    EditorStyle::Help(vPropCont, "History: 1 keeps the latest sample; 0 keeps all samples. DateTime uses Unix seconds.");
    auto* codeTitle = lv_label_create(vPropCont);
    lv_label_set_text(codeTitle, "C / C++ variable");
    lv_obj_set_style_text_color(codeTitle, lv_color_hex(EditorStyle::Accent), 0);
    varCodeTA = makeText("C/C++ symbol", "");
    lv_textarea_set_placeholder_text(varCodeTA, "Optional application variable, e.g. device_temperature");
    varCodeType = lv_dropdown_create(makeRow("C type"));
    lv_obj_set_width(varCodeType, 0);
    lv_obj_set_flex_grow(varCodeType, 1);
    EditorStyle::Control(varCodeType);
    EditorStyle::Help(vPropCont, "Bind your application's variable by address after ui_init(). Call ui_sync_variables() on the LVGL thread after your code changes it. Widget input writes back automatically.");
    varCodePreview = EditorStyle::Help(vPropCont, "Add a variable to see its C/C++ wiring.");
    auto* copy = lv_button_create(vPropCont);
    lv_obj_set_size(copy, 190, 36);
    EditorStyle::Button(copy);
    auto* copyText = lv_label_create(copy);
    lv_label_set_text(copyText, "Copy wiring code");
    EditorIcons::Set(copy, "copy", false, 18);
    lv_obj_add_event_cb(copy, [](lv_event_t* event) {
        auto* self = static_cast<ToolBar*>(lv_event_get_user_data(event));
        if (self->selectedVariable < 0) return;
        if (SDL_SetClipboardText(lv_label_get_text(self->varCodePreview)))
            self->VariableMessage("Wiring code copied. Include ui.h in your application.");
        else self->VariableMessage(std::string("Wiring code could not be copied: ") + SDL_GetError(), true);
    }, LV_EVENT_CLICKED, this);
    auto* widgetTitle = lv_label_create(vPropCont);
    lv_label_set_text(widgetTitle, "Widget connections");
    lv_obj_set_style_text_color(widgetTitle, lv_color_hex(EditorStyle::Accent), 0);
    varBindingSummary = EditorStyle::Help(vPropCont, "No variable selected.");
    // Keep the primary connection actions visible while the details scroll.
    auto* bindingActions = lv_cont_create(variableDetails, nullptr);
    lv_obj_move_to_index(bindingActions, 0);
    lv_obj_set_size(bindingActions, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bindingActions, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(bindingActions, 0, 0);
    lv_obj_set_style_border_width(bindingActions, 0, 0);
    lv_obj_set_style_bg_opa(bindingActions, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollable(bindingActions, false);
    for (bool selected : {false, true}) {
        auto* button = lv_button_create(bindingActions);
        lv_obj_set_size(button, selected ? 144 : 136, 36);
        EditorStyle::Button(button);
        auto* label = lv_label_create(button);
        lv_label_set_text(label, selected ? "Use selected" : "Add widget");
        EditorIcons::Set(button, selected ? "select-screen" : "add", false, 18);
        if (selected) selectedBindingBtn = button; else addBindingBtn = button;
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto* self = static_cast<ToolBar*>(lv_event_get_user_data(event));
            self->ShowVariableBinding(-1, lv_event_get_current_target_obj(event) == self->selectedBindingBtn);
        }, LV_EVENT_CLICKED, this);
    }
    varBindingList = lv_cont_create(vPropCont, nullptr);
    lv_obj_set_size(varBindingList, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(varBindingList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(varBindingList, 0, 0);
    lv_obj_set_style_border_width(varBindingList, 0, 0);
    lv_obj_set_style_bg_opa(varBindingList, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollable(varBindingList, false);
    varStatus = EditorStyle::Help(vPropCont, "Select or add a variable to connect application data to widgets.");
    for (auto* control : {varNameTA, varHistTA, varDefaultValTA, varDTDropDown, varCodeTA, varCodeType})
    {
        lv_obj_set_user_data(control, this);
        lv_obj_add_event_cb(control, [](lv_event_t* event) {
            auto* self = static_cast<ToolBar*>(lv_event_get_user_data(event));
            // Populating a selected variable is a view change, not a document
            // edit. Skip it before creating a history/serialization scope.
            if (self->loadingVariable) return;
            auto* field = lv_event_get_current_target_obj(event);
            const auto code = lv_event_get_code(event);
            EditorEdit::Scope edit(field, lv_obj_has_class(field, &lv_textarea_class), code == LV_EVENT_VALUE_CHANGED);
            updateVariable(field, code);
        }, LV_EVENT_ALL, this);
    }
    SelectVariable(-1);
    return;
#else
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
    SetEditorEventCallback<addVarCallback>(addVarBtn);

    removeVarBtn = lv_btn_create(btnCont, nullptr);
    lv_obj_set_size(removeVarBtn, 25, 35);
    lv_obj_t* lbl2 = lv_label_create(removeVarBtn, nullptr);
    lv_label_set_text(lbl2, LV_SYMBOL_MINUS);
    SetEditorEventCallback<removeVarCallback>(removeVarBtn);

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
    SetEditorHidden(addVarBtn, true);

    lv_obj_set_user_data(varNameTA, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<updateVariable>(varNameTA);
    lv_obj_set_user_data(varHistTA, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<updateVariable>(varHistTA);
    lv_obj_set_user_data(varDefaultValTA, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<updateVariable>(varDefaultValTA);
    lv_obj_set_user_data(varDTDropDown, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<updateVariable>(varDTDropDown);
#endif
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

void ToolBar::selectProperty(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
}

void ToolBar::addVarCallback(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
#if LVGL_VERSION_MAJOR >= 9
    auto* toolbar = static_cast<ToolBar*>(lv_obj_get_user_data(obj));
    toolbar->CommitVariableTargets();
    std::set<std::string> names;
    for (const auto& item : toolbar->variables) names.insert(item.at("var").value("name", std::string{}));
    unsigned index = 1;
    while (names.count("variable" + std::to_string(index))) ++index;
    toolbar->variables.push_back({{"var", {{"name", "variable" + std::to_string(index)},
        {"dataType", VAR_INT}, {"default", 0}, {"history", 1}, {"oid", -1}, {"property", ""}, {"bindings", json::array()}}}});
    toolbar->SelectVariable(static_cast<int>(toolbar->variables.size()) - 1);
    toolbar->RefreshVariables();
#endif
}

void ToolBar::removeVarCallback(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
#if LVGL_VERSION_MAJOR >= 9
    auto* toolbar = static_cast<ToolBar*>(lv_obj_get_user_data(obj));
    if (toolbar->selectedVariable < 0) return;
    toolbar->CommitVariableTargets();
    const auto name = toolbar->variables[toolbar->selectedVariable]["var"].value("name", std::string{});
    if (toolbar->variableRenamed) toolbar->variableRenamed(name, "");
    toolbar->variables.erase(toolbar->variables.begin() + toolbar->selectedVariable);
    toolbar->TrackVariableTargets();
    toolbar->SelectVariable(toolbar->variables.empty() ? -1 : 0);
    toolbar->RefreshVariables();
    toolbar->VariableMessage("Variable removed with its dependent actions. Undo restores it and its connections.");
#endif
}

void ToolBar::updateVariable(lv_obj_t* obj, EditorEventCode event)
{
#if LVGL_VERSION_MAJOR >= 9
    auto* toolbar = static_cast<ToolBar*>(lv_obj_get_user_data(obj));
    if (event == LV_EVENT_CLICKED)
    {
        FocusEditorControl(toolbar->propertyWindow->GetKBGroup(), obj);
    }
    if (event != LV_EVENT_VALUE_CHANGED || toolbar->loadingVariable || toolbar->selectedVariable < 0) return;
    auto& record = toolbar->variables[toolbar->selectedVariable]["var"];
    auto candidate = record;
    const auto previousType = record.value("dataType", 1u);
    const auto type = obj == toolbar->varDTDropDown ? lv_dropdown_get_selected(toolbar->varDTDropDown) : previousType;
    try
    {
        if (obj == toolbar->varNameTA) {
            const std::string name = lv_textarea_get_text(obj);
            if (name.empty()) throw std::runtime_error("Enter a unique variable name.");
            for (size_t i = 0; i < toolbar->variables.size(); ++i)
                if (int(i) != toolbar->selectedVariable && toolbar->variables[i]["var"].value("name", std::string{}) == name)
                    throw std::runtime_error("A variable already uses this name.");
            candidate["name"] = name;
        }
        if (obj == toolbar->varHistTA) {
            const std::string text = lv_textarea_get_text(obj);
            if (text.empty()) throw std::runtime_error("Enter a history length from 0 to 65535.");
            const auto history = std::stoul(text);
            if (history > 65535) throw std::runtime_error("History length must be from 0 to 65535.");
            candidate["history"] = history;
        }
        if (obj == toolbar->varDTDropDown && type != previousType) {
            candidate["dataType"] = type;
            candidate["default"] = type == VAR_STRING ? json("") : type == VAR_BOOL ? json(false) : type == VAR_FLOAT ? json(0.0) : json(0);
            candidate.erase("value");
            if (candidate.contains("code")) candidate["code"].erase("cType");
        }
        if (obj == toolbar->varDefaultValTA) {
            const std::string text = lv_textarea_get_text(obj);
            auto value = type == VAR_STRING ? json(text) : json::parse(text);
            candidate["default"] = value;
        }
        if (obj == toolbar->varCodeTA) {
            const std::string symbol = lv_textarea_get_text(obj);
            if (symbol.empty()) { if (candidate.contains("code")) candidate["code"].erase("symbol"); }
            else candidate["code"]["symbol"] = symbol;
        }
        if (obj == toolbar->varCodeType) {
            char selected[64]{};
            lv_dropdown_get_selected_str(obj, selected, sizeof selected);
            candidate["code"]["cType"] = selected;
        }
        const auto definition = Serialization::LVVariable::FromJSON({{"var", candidate}});
        if (type != previousType) for (const auto& binding : Serialization::LVVariable::Bindings(definition)) {
            const auto screens = Serialization::ProjectScreens::Multiple(toolbar->variableContext) ? toolbar->variableContext["screens"] : json::array({toolbar->variableContext});
            if (binding.screen < screens.size()) for (const auto& widget : screens[binding.screen].value("obj", json::array()))
                if (widget.value("oid", -1) == binding.objectId) Serialization::LVVariable::ValidateBinding(definition, binding, widget);
        }
        auto all = toolbar->variables;
        all[toolbar->selectedVariable] = {{"var", candidate}};
        Serialization::LVVariable::NormalizeList(all);
        const auto oldName = record.value("name", std::string{});
        const auto newName = candidate.value("name", std::string{});
        record = std::move(candidate);
        if (oldName != newName && toolbar->variableRenamed) toolbar->variableRenamed(oldName, newName);
        lv_obj_set_style_border_color(obj, lv_color_hex(EditorStyle::Border), 0);
        if (type != previousType) {
            toolbar->loadingVariable = true;
            const auto value = record.at("default");
            lv_textarea_set_text(toolbar->varDefaultValTA, (value.is_string() ? value.get<std::string>() : value.dump()).c_str());
            toolbar->loadingVariable = false;
            toolbar->RefreshVariableCodeTypes();
        }
        toolbar->VariableMessage("Saved. Try the connected widgets in Run mode.");
    }
    catch (const std::exception& error)
    {
        lv_obj_set_style_border_color(obj, lv_color_hex(EditorStyle::Error), 0);
        toolbar->VariableMessage(error.what(), true);
        if (obj == toolbar->varDTDropDown || obj == toolbar->varCodeType) {
            toolbar->loadingVariable = true;
            lv_dropdown_set_selected(toolbar->varDTDropDown, previousType);
            toolbar->RefreshVariableCodeTypes();
            toolbar->loadingVariable = false;
        }
    }
    toolbar->RefreshVariables();
    toolbar->RefreshVariableCode();
    return;
#endif
    if (event != LV_EVENT_VALUE_CHANGED)
        return;
}

void ToolBar::LoadVariables(const json& records)
{
    CloseVariableBinding();
    variables = Serialization::LVVariable::NormalizeList(records);
    TrackVariableTargets();
    SelectVariable(variables.empty() ? -1 : 0);
    RefreshVariables();
}

void ToolBar::SelectVariable(int index)
{
#if LVGL_VERSION_MAJOR >= 9
    selectedVariable = index;
    loadingVariable = true;
    const bool available = index >= 0 && static_cast<size_t>(index) < variables.size();
    for (auto* control : {varNameTA, varHistTA, varDefaultValTA, varDTDropDown, removeVarBtn, varCodeTA, varCodeType, addBindingBtn, selectedBindingBtn})
    {
        if (!control) continue;
        lv_obj_set_state(control, LV_STATE_DISABLED, !available);
        if (!available && lv_group_get_focused(propertyWindow->GetKBGroup()) == control)
            lv_group_remove_obj(control);
    }
    if (available)
    {
        const auto record = variables[index]["var"];
        lv_textarea_set_text(varNameTA, record.value("name", std::string{}).c_str());
        lv_textarea_set_text(varHistTA, std::to_string(record.value("history", 1)).c_str());
        lv_dropdown_set_selected(varDTDropDown, record.value("dataType", 1u));
        const auto value = record.value("default", json(""));
        lv_textarea_set_text(varDefaultValTA, (value.is_string() ? value.get<std::string>() : value.dump()).c_str());
        lv_textarea_set_text(varCodeTA, Serialization::LVVariable::CodeSymbol(Serialization::LVVariable::FromJSON(variables[index])).c_str());
    }
    else
    {
        lv_textarea_set_text(varNameTA, ""); lv_textarea_set_text(varHistTA, "1"); lv_textarea_set_text(varDefaultValTA, "");
        lv_textarea_set_text(varCodeTA, "");
    }
    loadingVariable = false;
    RefreshVariableCodeTypes();
    RefreshVariableCode();
    RefreshVariableBindings();
#endif
}

void ToolBar::RefreshVariables()
{
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_clean(variableList);
    for (size_t index = 0; index < variables.size(); ++index)
    {
        struct Selection { ToolBar* toolbar; int index; };
        auto* button = lv_button_create(variableList);
        lv_obj_set_size(button, LV_PCT(100), 54);
        lv_obj_set_style_pad_all(button, 6, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
        lv_obj_set_state(button, LV_STATE_CHECKED, static_cast<int>(index) == selectedVariable);
        auto* label = lv_label_create(button);
        lv_label_set_text(label, variables[index]["var"].value("name", std::string{}).c_str());
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_set_height(label, lv_font_get_line_height(lv_obj_get_style_text_font(label, LV_PART_MAIN)));
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
        auto* detail = lv_label_create(button);
        const auto definition = Serialization::LVVariable::FromJSON(variables[index]);
        const auto count = Serialization::LVVariable::Bindings(definition).size();
        const auto subtitle = std::to_string(count) + (count == 1 ? " widget  |  " : " widgets  |  ") + Serialization::LVVariable::CodeType(definition);
        lv_label_set_text(detail, subtitle.c_str());
        lv_obj_set_style_text_color(detail, lv_color_hex(EditorStyle::Muted), 0);
        lv_obj_set_style_text_font(detail, LV_FONT_DEFAULT, 0);
        lv_obj_set_width(detail, LV_PCT(100));
        lv_obj_set_height(detail, lv_font_get_line_height(lv_obj_get_style_text_font(detail, LV_PART_MAIN)));
        lv_label_set_long_mode(detail, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(detail, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto* selection = static_cast<Selection*>(lv_event_get_user_data(event));
            if (lv_event_get_code(event) == LV_EVENT_DELETE) { delete selection; return; }
            if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
            auto* toolbar = selection->toolbar;
            if (toolbar->variableSelected) toolbar->variableSelected();
            toolbar->SelectVariable(selection->index);
            lv_async_call([](void* context) { static_cast<ToolBar*>(context)->RefreshVariables(); }, toolbar);
        }, LV_EVENT_ALL, new Selection{this, static_cast<int>(index)});
    }
#endif
}

namespace {
using VariableDirection = Serialization::VariableBindingDirection;
json VariableScreens(const json& document)
{
    return Serialization::ProjectScreens::Multiple(document) ? document.at("screens") : json::array({document});
}
json VariableWidget(const json& document, size_t screen, int oid)
{
    const auto screens = VariableScreens(document);
    if (screen < screens.size()) for (const auto& widget : screens[screen].value("obj", json::array()))
        if (widget.value("oid", -1) == oid) return widget;
    return json::object();
}
json VariableContextView(const json& document)
{
    // Bindings need widget identity/type, not embedded images, fonts or tries.
    json result = {{"editorScreen", Serialization::ProjectScreens::Editing(document)}};
    auto compact = [](const json& screen) {
        json page = {{"name", screen.value("name", std::string("Screen 1"))}, {"obj", json::array()}};
        for (const auto& object : screen.value("obj", json::array())) {
            json widget = {{"oid", object.value("oid", -1)}, {"base", {{"name", object.value("base", json::object()).value("name", std::string("Widget"))}}}};
            for (const auto& property : object.items())
                if (property.key() != "base" && property.key() != "oid" && property.key() != "par") widget[property.key()] = json::object();
            const auto base = object.value("base", json::object());
            if (base.contains("flags") && base["flags"].is_object() && base["flags"].contains("checkable"))
                widget["base"]["flags"]["checkable"] = base["flags"]["checkable"];
            if (object.contains("button") && object["button"].is_object() && object["button"].contains("toggle"))
                widget["button"]["toggle"] = object["button"]["toggle"];
            page["obj"].push_back(std::move(widget));
        }
        return page;
    };
    if (Serialization::ProjectScreens::Multiple(document)) {
        result["screens"] = json::array();
        for (const auto& screen : document["screens"]) result["screens"].push_back(compact(screen));
    } else result["obj"] = compact(document)["obj"];
    return result;
}
std::string VariableCString(const std::string& value)
{
    std::ostringstream literal;
    literal << '"';
    for (unsigned char byte : value) {
        if (byte >= 32 && byte < 127 && byte != '"' && byte != '\\') literal << char(byte);
        else literal << '\\' << std::oct << std::setw(3) << std::setfill('0') << unsigned(byte) << std::dec;
    }
    return literal.str() + '"';
}
const char* VariableDirectionName(VariableDirection direction)
{
    return direction == VariableDirection::ToWidget ? "Display variable" :
        direction == VariableDirection::FromWidget ? "Input to variable" : "Read and write";
}
lv_obj_t* VariableRow(lv_obj_t* parent)
{
    auto* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_scrollable(row, false);
    return row;
}
lv_obj_t* VariableButton(lv_obj_t* parent, const char* text)
{
    auto* button = lv_button_create(parent);
    EditorStyle::Button(button);
    auto* label = lv_label_create(button);
    lv_label_set_text(label, text);
    // Fix the button width before adding a decoration: an icon layout must
    // not shrink a content-sized caption into an ellipsis during creation.
    lv_obj_update_layout(button);
    lv_obj_set_size(button, (std::max)(88, int(lv_obj_get_width(label)) + 48), 36);
    return button;
}
}

json ToolBar::VariablesToJSON() const
{
    auto result = variables;
#if LVGL_VERSION_MAJOR >= 9
    const auto objects = propertyWindow->GetObjectTree()->GetObjects();
    std::map<lv_obj_t*, int> ids;
    for (size_t i = 0; i < objects.size(); ++i) ids[objects[i]] = int(i);
    for (size_t i = 0; i < result.size(); ++i) {
        auto& record = result[i]["var"];
        const auto definition = Serialization::LVVariable::FromJSON(result[i]);
        const auto bindings = Serialization::LVVariable::Bindings(definition);
        auto remapped = json::array();
        for (size_t j = 0; j < bindings.size(); ++j) {
            auto binding = bindings[j];
            const auto tracked = variableTargets.find({i, j});
            if (tracked != variableTargets.end()) {
                const auto found = ids.find(tracked->second->Get());
                if (found == ids.end()) {
                    if (!record.contains("bindings")) { record["oid"] = -1; record["property"] = ""; }
                    continue;
                }
                binding.objectId = found->second;
                if (!record.contains("bindings")) record["oid"] = binding.objectId;
            }
            remapped.push_back(Serialization::LVVariable::BindingToJSON(binding));
        }
        if (record.contains("bindings")) record["bindings"] = std::move(remapped);
    }
#endif
    return Serialization::LVVariable::NormalizeList(result);
}

void ToolBar::TrackVariableTargets()
{
    variableTargets.clear();
#if LVGL_VERSION_MAJOR >= 9
    const auto screen = variableScreen ? variableScreen() : Serialization::ProjectScreens::Editing(variableContext);
    const auto objects = propertyWindow->GetObjectTree()->GetObjects();
    for (size_t i = 0; i < variables.size(); ++i) {
        const auto bindings = Serialization::LVVariable::Bindings(Serialization::LVVariable::FromJSON(variables[i]));
        for (size_t j = 0; j < bindings.size(); ++j) {
            const auto& binding = bindings[j];
            if (binding.screen == screen && binding.objectId >= 0 && size_t(binding.objectId) < objects.size())
                variableTargets[{i, j}] = std::make_shared<EditorObjectRef>(objects[binding.objectId]);
        }
    }
#endif
}

void ToolBar::CommitVariableTargets()
{
    variables = VariablesToJSON();
    TrackVariableTargets();
}

void ToolBar::RefreshVariableContext(const json& document, bool resetTargets)
{
    variableContext = VariableContextView(document);
    if (resetTargets) TrackVariableTargets();
    RefreshVariableBindings();
}

void ToolBar::VariableMessage(const std::string& message, bool error)
{
    if (!varStatus) return;
    lv_label_set_text(varStatus, message.c_str());
    lv_obj_set_style_text_color(varStatus, lv_color_hex(error ? EditorStyle::Error : EditorStyle::Muted), 0);
}

void ToolBar::RefreshVariableCodeTypes()
{
#if LVGL_VERSION_MAJOR >= 9
    if (!varCodeType) return;
    const bool previous = loadingVariable;
    loadingVariable = true;
    const auto definition = selectedVariable >= 0 ? Serialization::LVVariable::FromJSON(variables[selectedVariable]) : Serialization::VariableData{};
    const char* options = definition.type == Serialization::VariableType::String ? "char" :
        definition.type == Serialization::VariableType::Boolean ? "bool" :
        definition.type == Serialization::VariableType::Float ? "float\ndouble" :
        definition.type == Serialization::VariableType::DateTime ? "int64_t" : "int8_t\nint16_t\nint32_t\nint64_t\nuint8_t\nuint16_t\nuint32_t";
    lv_dropdown_set_options(varCodeType, options);
    std::istringstream lines(options);
    std::string type;
    uint32_t index = 0;
    const auto current = Serialization::LVVariable::CodeType(definition);
    while (std::getline(lines, type)) { if (type == current) { lv_dropdown_set_selected(varCodeType, index); break; } ++index; }
    loadingVariable = previous;
#endif
}

void ToolBar::RefreshVariableCode()
{
#if LVGL_VERSION_MAJOR >= 9
    if (!varCodePreview) return;
    if (selectedVariable < 0) { lv_label_set_text(varCodePreview, "Add a variable to see its C/C++ wiring."); return; }
    const auto definition = Serialization::LVVariable::FromJSON(variables[selectedVariable]);
    const auto id = CCodeGenerator::VariableIdentifier(variables, size_t(selectedVariable));
    auto symbol = Serialization::LVVariable::CodeSymbol(definition);
    const bool applicationSymbol = !symbol.empty();
    if (symbol.empty()) symbol = "application_" + id;
    const auto type = Serialization::LVVariable::CodeType(definition);
    std::string value = definition.defaultValue.dump();
    if (definition.type == Serialization::VariableType::String) value = VariableCString(definition.defaultValue.get<std::string>());
    if (value == "-9223372036854775808") value = "INT64_MIN";
    std::string code = "/* After ui_init(); keep storage alive while bound. */\n";
    if (applicationSymbol) code += "/* Existing application storage: " + type + " " + symbol +
        (definition.type == Serialization::VariableType::String ? "[]" : "") + "; */\n";
    if (definition.type == Serialization::VariableType::String) {
        if (!applicationSymbol) code += "static " + type + " " + symbol + "[" +
            std::to_string(std::max<size_t>(128, definition.defaultValue.get<std::string>().size() + 1)) + "] = " + value + ";\n";
        else code += "/* For a char pointer, pass its actual buffer capacity. */\n";
        code += "if (!ui_bind_" + id + "(" + symbol + ", sizeof " + symbol + ")) { /* handle error */ }\n";
    } else {
        if (!applicationSymbol) code += "static " + type + " " + symbol + " = " + value + ";\n";
        code += "if (!ui_bind_" + id + "(&" + symbol + ")) { /* handle error */ }\n";
    }
    code += "/* After application changes, on the LVGL thread: */\nif (!ui_sync_variables()) { /* handle error */ }";
    lv_label_set_text(varCodePreview, code.c_str());
#endif
}

void ToolBar::RefreshVariableBindings()
{
#if LVGL_VERSION_MAJOR >= 9
    if (!varBindingList) return;
    lv_obj_clean(varBindingList);
    if (selectedVariable < 0 || size_t(selectedVariable) >= variables.size()) {
        lv_label_set_text(varBindingSummary, "Select a variable to add widget connections.");
        return;
    }
    const auto definition = Serialization::LVVariable::FromJSON(variables[selectedVariable]);
    const auto bindings = Serialization::LVVariable::Bindings(definition);
    const auto summary = std::to_string(bindings.size()) + (bindings.size() == 1 ? " connected widget. " : " connected widgets. ") +
        "One value can drive several widgets, including widgets on other screens.";
    lv_label_set_text(varBindingSummary, summary.c_str());
    if (bindings.empty()) EditorStyle::Help(varBindingList, "No widgets connected. Add a widget or use the current canvas selection.");
    const auto screens = VariableScreens(variableContext);
    for (size_t i = 0; i < bindings.size(); ++i) {
        const auto& binding = bindings[i];
        const auto widget = VariableWidget(variableContext, binding.screen, binding.objectId);
        auto* card = lv_obj_create(varBindingList);
        lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_set_style_radius(card, 5, 0);
        lv_obj_set_scrollable(card, false);
        const auto screen = binding.screen < screens.size() ? screens[binding.screen].value("name", std::string("Screen 1")) : "Missing screen";
        const auto name = widget.value("base", json::object()).value("name", "Widget " + std::to_string(binding.objectId));
        auto* title = EditorStyle::Help(card, (screen + " / " + name).c_str());
        lv_obj_set_style_text_color(title, lv_color_hex(widget.empty() ? EditorStyle::Error : EditorStyle::Text), 0);
        auto property = binding.property;
        for (const auto& p : Serialization::LVVariable::BindingProperties(widget, definition.type))
            if (p.path == binding.property || p.path.substr(p.path.rfind('/') + 1) == binding.property.substr(binding.property.rfind('/') + 1)) { property = p.label; break; }
        EditorStyle::Help(card, (property + "  |  " + VariableDirectionName(binding.direction)).c_str());
        auto* actions = VariableRow(card);
        struct Action { ToolBar* self; size_t index; int kind; };
        for (int kind = 0; kind < 3; ++kind) {
            auto* button = VariableButton(actions, kind == 0 ? "Edit" : kind == 1 ? "Reveal" : "Remove");
            EditorIcons::Set(button, kind == 0 ? "view:properties" : kind == 1 ? "select-screen" : "variable-delete", false, 16);
            lv_obj_add_event_cb(button, [](lv_event_t* event) {
                auto* action = static_cast<Action*>(lv_event_get_user_data(event));
                if (lv_event_get_code(event) == LV_EVENT_DELETE) { delete action; return; }
                if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
                auto* self = action->self;
                const auto index = action->index;
                const auto kind = action->kind;
                if (kind == 0) { self->ShowVariableBinding(int(index)); return; }
                const auto bindings = Serialization::LVVariable::Bindings(Serialization::LVVariable::FromJSON(self->variables[self->selectedVariable]));
                if (index >= bindings.size()) return;
                if (kind == 1) { if (self->revealVariableTarget) self->revealVariableTarget(bindings[index].screen, bindings[index].objectId); return; }
                EditorEdit::Scope edit;
                self->CommitVariableTargets();
                auto updated = json::array();
                for (size_t j = 0; j < bindings.size(); ++j) if (j != index) updated.push_back(Serialization::LVVariable::BindingToJSON(bindings[j]));
                auto& record = self->variables[self->selectedVariable]["var"];
                record["bindings"] = std::move(updated); record["oid"] = -1; record["property"] = "";
                self->TrackVariableTargets();
                self->RefreshVariableBindings(); self->RefreshVariables();
                self->VariableMessage("Connection removed; the variable and widget are kept.");
            }, LV_EVENT_ALL, new Action{this, i, kind});
        }
    }
#endif
}

void ToolBar::CloseVariableBinding()
{
#if LVGL_VERSION_MAJOR >= 9
    if (bindingDialog) {
        auto* dialog = bindingDialog;
        bindingDialog = nullptr;
        lv_obj_delete(dialog);
    }
    bindingScreen = bindingWidget = bindingProperty = bindingDirection = bindingStatus = nullptr;
    bindingWidgetIds.clear(); bindingProperties.clear();
#endif
}

void ToolBar::ShowVariableBinding(int index, bool useSelected)
{
#if LVGL_VERSION_MAJOR >= 9
    if (selectedVariable < 0) return;
    CloseVariableBinding();
    CommitVariableTargets();
    if (variableDocument) RefreshVariableContext(variableDocument());
    editingBinding = index;
    const auto definition = Serialization::LVVariable::FromJSON(variables[selectedVariable]);
    const auto bindings = Serialization::LVVariable::Bindings(definition);
    Serialization::VariableBinding target;
    target.screen = variableScreen ? variableScreen() : Serialization::ProjectScreens::Editing(variableContext);
    if (index >= 0 && size_t(index) < bindings.size()) target = bindings[index];
    if (useSelected) {
        const auto objects = propertyWindow->GetObjectTree()->GetObjects();
        const auto found = std::find(objects.begin(), objects.end(), propertyWindow->GetSelectedObject());
        if (found == objects.end()) { VariableMessage("Select a widget on the canvas or in the hierarchy first.", true); return; }
        target.objectId = int(std::distance(objects.begin(), found));
        if (Serialization::LVVariable::BindingProperties(VariableWidget(variableContext, target.screen, target.objectId), definition.type).empty()) {
            VariableMessage("The selected widget has no compatible property for this variable type.", true); return;
        }
    }
    bindingDialog = lv_obj_create(lv_layer_top());
    lv_obj_set_size(bindingDialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(bindingDialog, lv_color_hex(0x111820), 0);
    lv_obj_set_style_bg_opa(bindingDialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(bindingDialog, 0, 0);
    lv_obj_set_style_radius(bindingDialog, 0, 0);
    lv_obj_set_scrollable(bindingDialog, false);
    auto* dialogWindow = EditorWindow::Create(bindingDialog);
    EditorWindow::SetTitle(dialogWindow, index < 0 ? "Add widget connection" : "Edit widget connection");
    EditorWindow::SetCloseHandler(dialogWindow, [this] { CloseVariableBinding(); });
    lv_obj_set_size(dialogWindow, std::min(600, lv_display_get_horizontal_resolution(nullptr) - 40),
        std::min(570, lv_display_get_vertical_resolution(nullptr) - 40));
    lv_obj_center(dialogWindow);
    auto* panel = EditorWindow::Content(dialogWindow);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_style_pad_all(panel, 16, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(EditorStyle::Surface), 0);
    EditorStyle::Help(panel, ("Variable: " + definition.name + ". Select the widget property and how data should flow.").c_str());
    auto field = [&](const char* name) {
        auto* row = VariableRow(panel);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        auto* label = lv_label_create(row);
        lv_label_set_text(label, name);
        lv_obj_set_width(label, 88);
        auto* dropdown = lv_dropdown_create(row);
        lv_obj_set_width(dropdown, 0); lv_obj_set_flex_grow(dropdown, 1);
        EditorStyle::Control(dropdown);
        lv_obj_add_event_cb(dropdown, [](lv_event_t* event) {
            auto* self = static_cast<ToolBar*>(lv_event_get_user_data(event));
            if (lv_event_get_code(event) == LV_EVENT_CLICKED) { FocusEditorControl(self->propertyWindow->GetKBGroup(), lv_event_get_current_target_obj(event)); return; }
            if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED || self->loadingBinding) return;
            if (lv_event_get_current_target_obj(event) == self->bindingScreen) self->RefreshBindingWidgets();
            else if (lv_event_get_current_target_obj(event) == self->bindingWidget) self->RefreshBindingProperties();
            else if (lv_event_get_current_target_obj(event) == self->bindingProperty) {
                const auto choice = lv_dropdown_get_selected(self->bindingProperty);
                if (choice < self->bindingProperties.size() && !self->bindingProperties[choice].readable)
                    lv_dropdown_set_selected(self->bindingDirection, 1);
            }
        }, LV_EVENT_ALL, this);
        return dropdown;
    };
    loadingBinding = true;
    bindingScreen = field("Screen");
    bindingWidget = field("Widget");
    bindingProperty = field("Property");
    bindingDirection = field("Direction");
    std::string screens;
    const auto records = VariableScreens(variableContext);
    for (size_t i = 0; i < records.size(); ++i) { if (i) screens += '\n'; screens += records[i].value("name", "Screen " + std::to_string(i + 1)); }
    lv_dropdown_set_options(bindingScreen, screens.c_str());
    lv_dropdown_set_selected(bindingScreen, target.screen < records.size() ? uint32_t(target.screen) : 0);
    lv_dropdown_set_options(bindingDirection, "Read and write\nDisplay variable\nInput to variable");
    lv_dropdown_set_selected(bindingDirection, target.direction == VariableDirection::ToWidget ? 1 : target.direction == VariableDirection::FromWidget ? 2 : 0);
    bindingStatus = EditorStyle::Help(panel, "Display variable sends values to the widget. Input to variable reads user edits. Read and write keeps both in sync.");
    auto* actions = VariableRow(panel);
    auto* save = VariableButton(actions, "Save connection");
    EditorIcons::Set(save, "save", false, 18);
    lv_obj_add_event_cb(save, [](lv_event_t* event) { static_cast<ToolBar*>(lv_event_get_user_data(event))->SaveVariableBinding(); }, LV_EVENT_CLICKED, this);
    auto* cancel = VariableButton(actions, "Cancel");
    lv_obj_add_event_cb(cancel, [](lv_event_t* event) { static_cast<ToolBar*>(lv_event_get_user_data(event))->CloseVariableBinding(); }, LV_EVENT_CLICKED, this);
    loadingBinding = false;
    RefreshBindingWidgets(target.objectId);
    RefreshBindingProperties(target.property);
    FocusEditorControl(propertyWindow->GetKBGroup(), bindingScreen);
#endif
}

void ToolBar::RefreshBindingWidgets(int selectedId)
{
#if LVGL_VERSION_MAJOR >= 9
    if (!bindingWidget) return;
    loadingBinding = true;
    bindingWidgetIds.clear();
    const auto screens = VariableScreens(variableContext);
    const auto screen = lv_dropdown_get_selected(bindingScreen);
    const auto definition = Serialization::LVVariable::FromJSON(variables[selectedVariable]);
    std::string options;
    uint32_t selected = 0;
    if (screen < screens.size()) for (const auto& widget : screens[screen].value("obj", json::array())) {
        if (Serialization::LVVariable::BindingProperties(widget, definition.type).empty()) continue;
        const auto id = widget.value("oid", -1);
        if (id < 0) continue;
        if (!options.empty()) options += '\n';
        options += widget.value("base", json::object()).value("name", "Widget " + std::to_string(id)) + " (#" + std::to_string(id) + ")";
        if (id == selectedId) selected = uint32_t(bindingWidgetIds.size());
        bindingWidgetIds.push_back(id);
    }
    lv_dropdown_set_options(bindingWidget, options.empty() ? "No compatible widgets" : options.c_str());
    lv_dropdown_set_selected(bindingWidget, selected);
    lv_obj_set_state(bindingWidget, LV_STATE_DISABLED, bindingWidgetIds.empty());
    loadingBinding = false;
    RefreshBindingProperties();
#endif
}

void ToolBar::RefreshBindingProperties(const std::string& path)
{
#if LVGL_VERSION_MAJOR >= 9
    if (!bindingProperty) return;
    loadingBinding = true;
    bindingProperties.clear();
    const auto choice = lv_dropdown_get_selected(bindingWidget);
    const auto definition = Serialization::LVVariable::FromJSON(variables[selectedVariable]);
    if (choice < bindingWidgetIds.size()) bindingProperties = Serialization::LVVariable::BindingProperties(
        VariableWidget(variableContext, lv_dropdown_get_selected(bindingScreen), bindingWidgetIds[choice]), definition.type);
    const auto widget = choice < bindingWidgetIds.size() ? VariableWidget(variableContext, lv_dropdown_get_selected(bindingScreen), bindingWidgetIds[choice]) : json::object();
    std::stable_sort(bindingProperties.begin(), bindingProperties.end(), [&](const auto& a, const auto& b) {
        auto priority = [&](const auto& property) {
            if (property.path == "/base/checked" && (widget.contains("switch") || widget.contains("cb"))) return 0;
            return property.path.rfind("/base/", 0) == 0 ? 10 : 0;
        };
        return priority(a) < priority(b);
    });
    std::string options;
    uint32_t selected = 0;
    for (size_t i = 0; i < bindingProperties.size(); ++i) {
        const auto& property = bindingProperties[i];
        if (i) options += '\n';
        options += property.label + (property.readable ? "" : " (display only)");
        if (!path.empty() && (property.path == path || property.path.substr(property.path.rfind('/') + 1) == path.substr(path.rfind('/') + 1))) selected = uint32_t(i);
    }
    lv_dropdown_set_options(bindingProperty, options.empty() ? "No compatible properties" : options.c_str());
    lv_dropdown_set_selected(bindingProperty, selected);
    lv_obj_set_state(bindingProperty, LV_STATE_DISABLED, bindingProperties.empty());
    if (!bindingProperties.empty() && !bindingProperties[selected].readable && lv_dropdown_get_selected(bindingDirection) == 0)
        lv_dropdown_set_selected(bindingDirection, 1);
    loadingBinding = false;
#endif
}

void ToolBar::SaveVariableBinding()
{
#if LVGL_VERSION_MAJOR >= 9
    if (!bindingDialog || selectedVariable < 0) return;
    try {
        const auto selectedWidget = lv_dropdown_get_selected(bindingWidget), selectedProperty = lv_dropdown_get_selected(bindingProperty);
        if (selectedWidget >= bindingWidgetIds.size() || selectedProperty >= bindingProperties.size()) throw std::runtime_error("Select a compatible widget and property.");
        auto definition = Serialization::LVVariable::FromJSON(variables[selectedVariable]);
        auto bindings = Serialization::LVVariable::Bindings(definition);
        Serialization::VariableBinding binding;
        if (editingBinding >= 0 && size_t(editingBinding) < bindings.size()) binding = bindings[size_t(editingBinding)];
        binding.screen = lv_dropdown_get_selected(bindingScreen);
        binding.objectId = bindingWidgetIds[selectedWidget];
        binding.property = bindingProperties[selectedProperty].path;
        const auto direction = lv_dropdown_get_selected(bindingDirection);
        if (direction != 1 && !bindingProperties[selectedProperty].readable)
            throw std::runtime_error("This property is display only. Select Display variable for its direction.");
        binding.direction = direction == 1 ? VariableDirection::ToWidget : direction == 2 ? VariableDirection::FromWidget : VariableDirection::TwoWay;
        Serialization::LVVariable::ValidateBinding(definition, binding, VariableWidget(variableContext, binding.screen, binding.objectId));
        auto entries = json::array();
        for (size_t i = 0; i < bindings.size(); ++i) entries.push_back(Serialization::LVVariable::BindingToJSON(int(i) == editingBinding ? binding : bindings[i]));
        if (editingBinding < 0) entries.push_back(Serialization::LVVariable::BindingToJSON(binding));
        auto candidate = variables;
        auto& record = candidate[selectedVariable]["var"];
        record["bindings"] = entries; record["oid"] = -1; record["property"] = "";
        auto normalized = Serialization::LVVariable::NormalizeList(candidate);
        EditorEdit::Scope edit;
        variables = std::move(normalized);
        TrackVariableTargets();
        CloseVariableBinding();
        RefreshVariableBindings(); RefreshVariables();
        VariableMessage("Connection saved. Use Run mode to test data flowing between the widgets.");
    } catch (const std::exception& error) {
        lv_label_set_text(bindingStatus, error.what());
        lv_obj_set_style_text_color(bindingStatus, lv_color_hex(EditorStyle::Error), 0);
    }
#endif
}
