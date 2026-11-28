#include "AdvancedControls.h"
#include "EditorInputTest.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/ObjectIcons.h"
#include <Serialization/AdvancedControls.h>
#include <Serialization/Protobuf.h>
#include <iostream>

namespace {
namespace Input = EditorInputTest;
using Serialization::AdvancedControls;
lv_obj_t* Field(PropertyWindow* properties, const char* caption, const lv_obj_class_t* type = &lv_textarea_class)
{
    auto* control = Input::FindField(properties->ObjectPropWin()->GetWindow(), caption, type);
    Input::Require(control, std::string("Advanced control inspector field missing: ") + caption);
    return control;
}
lv_obj_t* Check(PropertyWindow* properties, const char* caption)
{
    const auto find = [&](auto&& self, lv_obj_t* object) -> lv_obj_t* {
        if (lv_obj_has_class(object, &lv_checkbox_class) && std::string(lv_checkbox_get_text(object)) == caption) return object;
        for (uint32_t index = 0; index < lv_obj_get_child_count(object); ++index)
            if (auto* match = self(self, lv_obj_get_child(object, index))) return match;
        return nullptr;
    };
    auto* result = find(find, properties->ObjectPropWin()->GetWindow());
    Input::Require(result, std::string("Advanced control checkbox property missing: ") + caption);
    return result;
}
void Choose(lv_obj_t* control, uint32_t index)
{
    lv_dropdown_set_selected(control, index);
    lv_obj_send_event(control, LV_EVENT_VALUE_CHANGED, nullptr);
}
void Click(lv_obj_t* control)
{
    Input::Require(control, "Advanced control runtime part is missing");
    lv_obj_send_event(control, LV_EVENT_CLICKED, nullptr);
}
lv_obj_t* GroupContent(PropertyWindow* properties, const char* title)
{
    auto* label = Input::FindLabel(properties->ObjectPropWin()->GetWindow(), title);
    Input::Require(label, std::string("Advanced inspector group missing: ") + title);
    auto* button = lv_obj_get_parent(label);
    return lv_obj_get_child(lv_obj_get_parent(button), 1);
}
}

void CheckAdvancedControlsEditor(UI& editor, SdlSimulator& simulator, const std::filesystem::path& capture)
{
    auto document = editor.ProjectDocument();
    document["canvas"]["width"] = 480; document["canvas"]["height"] = 480;
    document["obj"] = json::array(); document["variables"] = json::array();
    editor.ApplyDocument(document);
    auto* tray = Input::WindowFromTitle("Tool Tray");
    Input::Require(tray, "Tool Tray is missing");
    auto* palette = EditorWindow::Content(tray);
    auto* tab = Input::FindLabel(palette, "Advanced\nWidgets");
    Input::Require(tab, "Advanced Widgets category is missing"); Click(lv_obj_get_parent(tab));
    static const char* names[] = {"Radio Group", "Checkbox", "Checkbox nested in Label", "Date Picker", "Menu"};
    static const char* types[] = {"radioGroup", "checkboxButton", "labeledCheckbox", "datePicker", "menu"};
    PropertyWindow* properties = nullptr;
    for (int index = 0; index < 5; ++index)
    {
        editor.MenuAction("select-screen");
        auto* caption = Input::FindLabel(palette, names[index]);
        Input::Require(caption, std::string("Advanced palette entry missing: ") + names[index]);
        Click(lv_obj_get_parent(caption));
        const auto project = editor.ProjectDocument();
        Input::Require(project["obj"].size() == static_cast<size_t>(index + 1), "Composite internals became authored objects");
        Input::Require(project["obj"][index].contains(types[index]), "Advanced palette created the wrong widget type");
        if (!properties)
        {
            auto* title = Input::FindLabel(lv_screen_active(), "Simulator [Size:480x480]");
            Input::Require(title, "Resized simulator is missing");
            auto* surface = lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))), 0);
            properties = static_cast<ObjectUserData*>(lv_obj_get_user_data(lv_obj_get_child(surface, 0)))->toolTray->GetPropertyWindow();
        }
        auto* object = properties->GetSelectedObject();
        Input::Require(std::string(AdvancedControls::Type(object)) == types[index], "Advanced control type was lost");
        Input::Require(ObjectIcons::ForObject(object) == ObjectIcons::Widget(35 + index), "Advanced hierarchy icon differs from its palette icon");
    }
    auto* ordinaryLabel = Input::FindLabel(palette, "Label");
    Input::Require(ordinaryLabel, "Basic Label palette entry is missing"); Click(lv_obj_get_parent(ordinaryLabel));
    auto sibling = editor.ProjectDocument();
    Input::Require(sibling["obj"].size() == 6 && sibling["obj"][5]["par"] == -1, "Adding beside a selected composite nested an authored object into generated menu content");
    Input::Require(editor.Undo() && editor.ProjectDocument()["obj"].size() == 5, "Composite sibling insertion was not one undo step");
    auto object = [&](int index) { return properties->GetObjectTree()->GetObjects().at(index); };
    properties->SetSelectedObject(object(0));
    auto* selected = Field(properties, "Selected option", &lv_dropdown_class);
    Input::Require(lv_dropdown_get_option_count(selected) == 4, "Radio choices were not exposed as named options with a None choice");
    Choose(selected, 2);
    Input::Require(editor.ProjectDocument()["obj"][0]["radioGroup"]["selected"] == 1, "Radio selection property was not applied");
    Input::Require(editor.Undo() && editor.ProjectDocument()["obj"][0]["radioGroup"]["selected"] == 0, "Radio property change was not undoable");
    Input::Require(editor.Redo() && editor.ProjectDocument()["obj"][0]["radioGroup"]["selected"] == 1, "Radio property redo lost its selection");
    auto* options = GroupContent(properties, "Options");
    auto* addOption = Input::FindLabel(options, "Add"); Input::Require(addOption, "Radio Add option action is missing");
    Click(lv_obj_get_parent(addOption)); Input::Drain(simulator);
    Input::Require(editor.ProjectDocument()["obj"][0]["radioGroup"]["options"].size() == 4, "Radio Add action did not create an editable option");
    Input::Require(editor.Undo() && editor.ProjectDocument()["obj"][0]["radioGroup"]["options"].size() == 3, "Adding a radio option was not undoable");

    properties->SetSelectedObject(object(1));
    Input::Replace(Field(properties, "Text"), "Remember my choice", simulator);
    Input::Require(editor.ProjectDocument()["obj"][1]["checkboxButton"]["text"] == "Remember my choice", "Advanced Checkbox caption cannot be typed");
    auto* checked = Check(properties, "Checked"); lv_obj_set_checked(checked, true); lv_obj_send_event(checked, LV_EVENT_VALUE_CHANGED, nullptr);
    Input::Require(editor.ProjectDocument()["obj"][1]["checkboxButton"]["checked"] == true, "Advanced Checkbox checked property was not applied");

    properties->SetSelectedObject(object(2));
    Input::Replace(Field(properties, "Supporting label"), "Your preference is saved on this device.", simulator);
    Input::Require(editor.ProjectDocument()["obj"][2]["labeledCheckbox"]["description"] == "Your preference is saved on this device.", "Nested Checkbox supporting label cannot be typed");

    properties->SetSelectedObject(object(3));
    Input::Replace(Field(properties, "Date (YYYY-MM-DD)"), "2028-02-29", simulator);
    Choose(Field(properties, "Date format", &lv_dropdown_class), 1);
    Choose(Field(properties, "First day of week", &lv_dropdown_class), 1);
    auto configured = editor.ProjectDocument();
    Input::Require(configured["obj"][3]["datePicker"]["date"] == "2028-02-29" && configured["obj"][3]["datePicker"]["format"] == "dd/mm/yyyy" && configured["obj"][3]["datePicker"]["firstDay"] == 1,
        "Date Picker typed date, format, or first-day property was not persisted");
    auto* date = Field(properties, "Date (YYYY-MM-DD)"); lv_textarea_set_text(date, "2028-02-30"); lv_obj_send_event(date, LV_EVENT_VALUE_CHANGED, nullptr);
    Input::Require(editor.ProjectDocument() == configured, "Invalid calendar date changed the design");

    properties->SetSelectedObject(object(4));
    auto* selectedItem = Field(properties, "Selected item", &lv_dropdown_class);
    Input::Require(lv_dropdown_get_option_count(selectedItem) == 3, "Menu selection should offer None and enabled leaf items");
    Choose(selectedItem, 2);
    Input::Require(editor.ProjectDocument()["obj"][4]["menu"]["selected"] == 3, "Named menu selection did not map to its stable item ID");
    auto* items = GroupContent(properties, "Items");
    auto* branch = Input::FindLabel(items, "Electronics (#2)"); Input::Require(branch, "Menu branch inspector group is missing");
    auto* branchContent = lv_obj_get_child(lv_obj_get_parent(lv_obj_get_parent(branch)), 1);
    auto* parent = Input::FindField(branchContent, "Parent", &lv_dropdown_class);
    Input::Require(parent && lv_dropdown_get_option_count(parent) == 2, "Menu parent choices include itself or its descendants");
    auto* branchDisabled = Input::FindClass(branchContent, &lv_checkbox_class);
    Input::Require(branchDisabled, "Menu branch disabled property is missing");
    lv_obj_set_checked(branchDisabled, true); lv_obj_send_event(branchDisabled, LV_EVENT_VALUE_CHANGED, nullptr); Input::Drain(simulator);
    Input::Require(lv_dropdown_get_option_count(Field(properties, "Selected item", &lv_dropdown_class)) == 2 && editor.ProjectDocument()["obj"][4]["menu"]["selected"] == 0,
        "Disabling a menu branch left a selected or selectable descendant");
    Input::Require(editor.Undo(), "Disabling a menu branch was not undoable");
    items = GroupContent(properties, "Items"); branch = Input::FindLabel(items, "Electronics (#2)");
    Input::Require(branch, "Undo did not restore the menu branch inspector");
    branchContent = lv_obj_get_child(lv_obj_get_parent(lv_obj_get_parent(branch)), 1);
    auto* remove = Input::FindLabel(branchContent, "Remove"); Input::Require(remove, "Menu Remove branch action is missing");
    Click(lv_obj_get_parent(remove)); Input::Drain(simulator);
    Input::Require(editor.ProjectDocument()["obj"][4]["menu"]["items"].size() == 1, "Removing a menu branch left its orphaned descendants");
    Input::Require(editor.Undo() && editor.ProjectDocument()["obj"][4]["menu"]["items"].size() == 3, "Removing a menu branch was not undoable");

    // Keep all controls visible in a single runtime preview, with no authored children.
    configured = editor.ProjectDocument();
    const int positions[][4] = {{12, 12, 450, 100}, {12, 126, 200, 44}, {228, 126, 240, 96}, {12, 226, 200, 44}, {228, 226, 240, 220}};
    for (int index = 0; index < 5; ++index)
    {
        auto& base = configured["obj"][index]["base"];
        base["coords"] = {{"x", positions[index][0]}, {"y", positions[index][1]}, {"width", positions[index][2]}, {"height", positions[index][3]}};
        // Palette snapshots include explicit LVGL geometry styles, which are
        // restored after coords. Keep their appearance while replacing layout.
        if (base.contains("nativeStyles"))
            for (auto& style : base["nativeStyles"])
                if (style.contains("values") && style["values"].is_object())
                    for (const char* property : {"x", "y", "width", "height"}) style["values"].erase(property);
    }
    editor.ApplyDocument(configured);
    const auto authored = editor.ProjectDocument();
    for (int index = 0; index < 5; ++index)
        Input::Require(authored["obj"][index]["base"]["coords"] == configured["obj"][index]["base"]["coords"],
            "Advanced control preview arrangement was overridden by saved geometry styles");
    Click(AdvancedControls::Part(object(0), "Option2")); Click(object(1));
    Click(AdvancedControls::Part(object(3), "Display")); Click(AdvancedControls::Part(object(4), "Item2"));
    Input::Require(editor.ProjectDocument() == authored && !AdvancedControls::Part(object(3), "Popup"), "Design clicks interacted with runtime controls instead of selecting them");
    Input::Require(editor.HandleShortcut(SDLK_F6, false) && editor.Running(), "F6 did not start the advanced controls runtime");
    Input::Drain(simulator);
    auto* runtime = editor.RunRuntime();
    auto* radio = lvgl_runtime_object(runtime, 0, 0);
    Click(AdvancedControls::Part(radio, "Option2"));
    Input::Require(AdvancedControls::ToJSON(radio)["radioGroup"]["selected"] == 2, "F6 radio options are not exclusive/selectable");
    auto* checkbox = lvgl_runtime_object(runtime, 0, 1); Click(checkbox);
    Input::Require(AdvancedControls::ToJSON(checkbox)["checkboxButton"]["checked"] == false, "F6 advanced Checkbox does not toggle");
    auto* nested = lvgl_runtime_object(runtime, 0, 2); Click(nested);
    Input::Require(AdvancedControls::ToJSON(nested)["labeledCheckbox"]["checked"] == true, "F6 nested-label Checkbox does not toggle from its card");
    auto* picker = lvgl_runtime_object(runtime, 0, 3);
    Input::Require(!AdvancedControls::Part(picker, "Popup"), "Date Picker eagerly allocated its calendar popup");
    Click(AdvancedControls::Part(picker, "Display"));
    Input::Require(AdvancedControls::Part(picker, "Calendar") && AdvancedControls::Part(picker, "Popup"), "F6 Date Picker did not open a calendar");
    ui_adv_date_close(picker);
    auto* menu = lvgl_runtime_object(runtime, 0, 4);
    Click(AdvancedControls::Part(menu, "Item1"));
    Input::Require(AdvancedControls::ToJSON(menu)["menu"]["selected"] == 1, "F6 menu did not select its top-level leaf item");
    Click(AdvancedControls::Part(menu, "Item2")); Click(AdvancedControls::Part(menu, "Item3"));
    Input::Require(AdvancedControls::ToJSON(menu)["menu"]["selected"] == 3, "F6 nested menu did not select its leaf item");
    Input::Require(editor.ProjectDocument() == authored, "F6 controls modified their authored defaults");
    if (!capture.empty())
    {
        Input::Drain(simulator);
        auto* renderer = static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
        auto* image = SDL_RenderReadPixels(renderer, nullptr); Input::Require(image, "Cannot capture Advanced Widgets preview");
        const bool saved = SDL_SaveBMP(image, capture.u8string().c_str()); SDL_DestroySurface(image);
        Input::Require(saved, "Cannot save Advanced Widgets preview");
    }
    editor.MenuAction("run-stop"); Input::Drain(simulator);
    Input::Require(editor.ProjectDocument() == authored, "Stopping F6 controls changed the project");
    const auto protobuf = Serialization::Protobuf::Encode(Serialization::EntityKind::Project, "project", authored);
    Input::Require(Serialization::Protobuf::Decode(protobuf).data == authored, "Advanced editor settings did not round-trip through Protobuf");
    std::cout << "Advanced controls: five palette entries/icons, typed inspector fields, named choices, validation, undo/redo, F6 interactions/isolation and persistence passed\n";
}
