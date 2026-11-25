#include "TextInputModes.h"
#include "EditorInputTest.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Windows/PropertyWindows/PropertyControls.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include <Serialization/CustomWidgets.h>
#include <lv_advanced_keyboard.h>
#include <lv_textarea_input_mode.h>
#include <iostream>

namespace {
namespace Input = EditorInputTest;
json Widget(int id, const char* section, json data, int y)
{
    const bool keyboard = std::string(section) == "advancedKeyboard";
    return {{"oid", id}, {"par", -1}, {"base", {{"name", std::string("Mode ") + section},
        {"coords", {{"x", 12}, {"y", y}, {"width", keyboard ? 450 : 280}, {"height", keyboard ? 220 : 48}}}}}, {section, std::move(data)}};
}

lv_obj_t* Mode(PropertyWindow* properties)
{
    auto* control = Input::FindField(properties->ObjectPropWin()->GetWindow(), "Input mode", &lv_dropdown_class);
    Input::Require(control, "Text field input mode is missing from the inspector");
    Input::Require(std::string(lv_dropdown_get_options(control)) == "Alpha Numeric\nUpper case\nLower case\nNumeric",
        "Text field input mode dropdown labels are incorrect");
    return control;
}

void Choose(PropertyWindow* properties, uint32_t choice, SdlSimulator& simulator)
{
    auto* field = Mode(properties);
    lv_dropdown_set_selected(field, choice);
    lv_obj_send_event(field, LV_EVENT_VALUE_CHANGED, nullptr);
    Input::Drain(simulator);
}
}

void CheckTextInputModes(UI& editor, SdlSimulator& simulator)
{
    const json fixture{{"formatVersion", 2}, {"lvglVersion", "9.6.0"},
        {"canvas", {{"width", 480}, {"height", 400}}}, {"obj", json::array({
            Widget(0, "textarea", {{"text", "Mixed Case 12.3!"}, {"oneLine", true}}, 12),
            Widget(1, "textBox", {{"text", "Mixed Case 12.3!"}, {"numeric", false}, {"showButtons", false}}, 72),
            Widget(2, "advancedKeyboard", {{"language", "en-US"}, {"trieId", ""}, {"autoFocus", true}}, 144)})}};
    editor.ApplyDocument(fixture);
    Input::Drain(simulator);
    // Composite widgets materialize their resolved part styles on the first
    // project reload. Start from that canonical fixture so Undo comparisons
    // cover input edits rather than unrelated initial theme materialization.
    editor.ApplyDocument(editor.ProjectDocument());
    editor.SelectObjectIds({0}); Input::Drain(simulator);
    auto* propertiesWindow = Input::WindowFromTitle("Properties");
    Input::Require(propertiesWindow, "Properties window is missing");
    auto* x = Input::FindField(propertiesWindow, "X");
    Input::Require(x, "Base coordinate field is missing");
    auto* properties = static_cast<sPropChange*>(lv_obj_get_user_data(x))->pw;
    for (int id : {0, 1})
    {
        editor.SelectObjectIds({id}); Input::Drain(simulator);
        const auto beforeSelection = editor.ProjectDocument();
        const auto revision = editor.DocumentRevision();
        const auto historyBytes = editor.HistoryLogicalBytes();
        properties->SetSelectedObject(properties->GetSelectedObject()); Input::Drain(simulator);
        if (editor.ProjectDocument() != beforeSelection || editor.HistoryLogicalBytes() != historyBytes)
            std::cerr << "Selection refresh: id=" << id << " revision=" << revision << "->" << editor.DocumentRevision()
                << " history=" << historyBytes << "->" << editor.HistoryLogicalBytes() << " diff="
                << json::diff(beforeSelection, editor.ProjectDocument()).dump().substr(0, 3000) << '\n';
        // Selection outlines invalidate the document serialization cache, so
        // its revision can advance without an authored change.
        Input::Require(editor.ProjectDocument() == beforeSelection && editor.HistoryLogicalBytes() == historyBytes,
            "Selecting a text box mutated its input mode or document history");
        Input::Require(lv_dropdown_get_selected(Mode(properties)) == 0, "Legacy text box did not default to unrestricted input");
        for (uint32_t mode : {1u, 2u, 3u, 0u})
        {
            const auto before = editor.ProjectDocument();
            Choose(properties, mode, simulator);
            const auto after = editor.ProjectDocument();
            const char* section = id == 0 ? "textarea" : "textBox";
            const char* name = mode == 1 ? "uppercase" : mode == 2 ? "lowercase" : mode == 3 ? "numeric" : "alphanumeric";
            Input::Require(after["obj"][id][section].value("inputMode", std::string("alphanumeric")) == name,
                "Inspector did not persist the selected text box input mode");
            const bool undone = editor.Undo();
            if (!undone || editor.ProjectDocument() != before)
                std::cerr << "Mode undo: id=" << id << " mode=" << mode << " undone=" << undone << " diff="
                    << json::diff(before, editor.ProjectDocument()).dump().substr(0, 3000) << '\n';
            Input::Require(undone && editor.ProjectDocument() == before, "Text input mode and text normalization were not undoable together");
            const bool redone = editor.Redo();
            if (!redone || editor.ProjectDocument() != after)
                std::cerr << "Mode redo: id=" << id << " mode=" << mode << " redone=" << redone << " diff="
                    << json::diff(after, editor.ProjectDocument()).dump().substr(0, 3000) << '\n';
            Input::Require(redone && editor.ProjectDocument() == after, "Text input mode and normalized text were not redoable together");
            editor.SelectObjectIds({id}); Input::Drain(simulator);
            auto* root = properties->GetSelectedObject();
            auto* entry = id == 0 ? root : Serialization::CustomWidgets::Part(root, "Entry");
            Input::Require(lv_advanced_keyboard_get_textarea_input_mode(entry) == static_cast<lv_advanced_keyboard_input_mode_t>(mode),
                "Restored text box lost its per-target input mode");
            // Use real SDL typing to exercise the same path as a hardware keyboard.
            lv_textarea_set_text(entry, "");
            editor.SelectObjectIds({id}); Input::Drain(simulator);
            Input::Text(u8"aBéЖ1.2z.3!", simulator);
            const std::string expected = mode == 1 ? u8"ABÉЖ1.2Z.3!" : mode == 2 ? u8"abéж1.2z.3!" :
                mode == 3 ? "1.23" : u8"aBéЖ1.2z.3!";
            Input::Require(std::string(lv_textarea_get_text(entry)) == expected,
                "Physical typing bypassed the selected text input mode");
            editor.SelectObjectIds({2}); Input::Drain(simulator);
            auto* keyboard = properties->GetSelectedObject();
            editor.SelectObjectIds({id}); Input::Drain(simulator);
            Input::Require(lv_advanced_keyboard_get_textarea(keyboard) == entry && !lv_obj_is_hidden(keyboard),
                "Switching text boxes did not retarget the Advanced Keyboard");
            auto* keys = lv_advanced_keyboard_get_keyboard(keyboard);
            if (mode == 3)
            {
                for (const char* digit : {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "."})
                    Input::Require(Input::FindLabel(keys, digit), "Numeric keyboard is missing a digit or decimal point");
                Input::Require(!Input::FindLabel(keys, "a") && !Input::FindLabel(keys, "A"),
                    "Numeric keyboard still exposes letter keys");
            }
            else Input::Require(Input::FindLabel(keys, mode == 1 ? "A" : "a"),
                "Advanced Keyboard letter case did not follow the selected target");
        }
    }
    std::cout << "Text input modes: dropdown defaults, normalized Undo/Redo, physical Unicode input, numeric filtering and per-target keyboard switching passed\n";
}
