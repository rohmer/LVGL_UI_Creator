#include "LanguageModels.h"
#include "../../../UICreator/LanguageModels/LanguageModelWindow.h"
#include "../../../UICreator/Utils/EditorSession.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Windows/UI.h"
#include <lv_advanced_keyboard_prediction.h>
#include <SDL3/SDL.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
lv_obj_t* Label(lv_obj_t* parent, const char* text) {
    if (lv_obj_get_class(parent) == &lv_label_class && std::string(lv_label_get_text(parent)) == text) return parent;
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) if (auto* found = Label(lv_obj_get_child(parent, i), text)) return found;
    return nullptr;
}
lv_obj_t* Field(lv_obj_t* parent, const char* caption) {
    auto* title = Label(parent, caption); Require(title, "Language model generator input label missing");
    auto* field = lv_obj_get_child(lv_obj_get_parent(title), 1);
    Require(field && (lv_obj_has_class(field, &lv_textarea_class) || lv_obj_has_class(field, &lv_dropdown_class)), "Language model row does not contain an input");
    return field;
}
void Text(lv_obj_t* parent, const char* caption, const std::string& value) { lv_textarea_set_text(Field(parent, caption), value.c_str()); }
void Click(lv_obj_t* parent, const char* caption) {
    auto* title = Label(parent, caption); Require(title, "Language model action missing");
    auto* button = lv_obj_get_parent(title); Require(lv_obj_has_class(button, &lv_button_class), "Language model action is not a button");
    Require(!lv_obj_has_state(button, LV_STATE_DISABLED), "Language model action is disabled");
    lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
}
bool HasText(lv_obj_t* parent, const char* needle) {
    if (lv_obj_get_class(parent) == &lv_label_class && std::string(lv_label_get_text(parent)).find(needle) != std::string::npos) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) if (HasText(lv_obj_get_child(parent, i), needle)) return true;
    return false;
}
void Write(const fs::path& path, const std::string& value) {
    std::ofstream output(path, std::ios::binary); output.write(value.data(), static_cast<std::streamsize>(value.size()));
    Require(bool(output), "Cannot write model editor test corpus");
}
void Wait(SdlSimulator& simulator, bool closing = false) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (LanguageModelWindow::Running() || (closing && LanguageModelWindow::ActiveWindow())) {
        Require(std::chrono::steady_clock::now() < deadline, "Language model background job did not finish/cancel");
        SDL_Delay(10); simulator.ProcessEvents();
    }
}
}

void CheckLanguageModelsEditor(UI& editor, SdlSimulator& simulator) {
    const auto work = fs::absolute("language-model-editor-work") / ("run-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(work / "corpus");
    const auto session = work / "session";
#ifdef _WIN32
    Require(SetEnvironmentVariableW(L"UICREATOR_SESSION_DIR", session.c_str()) != 0, "Cannot isolate generator settings");
#else
    Require(setenv("UICREATOR_SESSION_DIR", session.c_str(), 1) == 0, "Cannot isolate generator settings");
#endif
    Write(work / "corpus/input.txt", "the world. the world. hello world. hello there. help the hero.\nin the world.\n");
    Require(Label(lv_screen_active(), "Language Models"), "Top-level Language Models menu missing");
    auto* trayTitle = Label(lv_screen_active(), "Tool Tray"); Require(trayTitle, "Tool Tray missing");
    auto* palette = EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(trayTitle)));
    Click(palette, "Text area"); editor.MenuAction("select-screen"); Click(palette, "Advanced\nWidgets"); Click(palette, "Advanced Keyboard");
    auto* title = Label(lv_screen_active(), "Simulator [Size:320x240]"); Require(title, "Simulator missing");
    auto* surface = lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))), 0);
    auto* keyboard = lv_obj_get_child(surface, 1); Require(lv_advanced_keyboard_is_instance(keyboard), "Cannot create language model target keyboard");
    auto* properties = static_cast<ObjectUserData*>(lv_obj_get_user_data(keyboard))->toolTray->GetPropertyWindow();
    properties->SetSelectedObject(keyboard);
    const auto before = editor.ProjectDocument();
    editor.MenuAction("language-model-generator");
    auto* window = LanguageModelWindow::ActiveWindow(); Require(window, "Generator menu action did not open the LVGL window");
    Require(lv_dropdown_get_option_count(Field(window, "Corpus language")) == 12, "Corpus language choices differ from the C# generator");
    Text(window, "Corpus folders (one per line)", (work / "corpus").u8string());
    Text(window, "Trie output file", (work / "generated.ngtr").u8string());
    Text(window, "Maximum trie bytes", "16384"); Text(window, "Worker threads", "0");
    Click(window, "Generate trie");
    Require(!LanguageModelWindow::Running() && !fs::exists(work / "generated.ngtr"), "Invalid generator settings started a job");
    Text(window, "Worker threads", "2"); Click(window, "Generate trie");
    Require(LanguageModelWindow::Running(), "Generate button did not start native background work");
    Wait(simulator);
    Require(fs::is_regular_file(work / "generated.ngtr") && HasText(window, "Saved "), "Generator did not save a model or show completion");
    Require(editor.ProjectDocument() == before, "Generating a file changed the authored project before import");
    Click(window, "Load into project");
    const auto imported = editor.ProjectDocument();
    const auto generatedId = imported["obj"][1]["advancedKeyboard"]["trieId"];
    bool hasGenerated = false;
    for (const auto& model : imported["tries"]) if (model["id"] == generatedId) hasGenerated = true;
    Require(imported["tries"].size() == before["tries"].size() + 1 && hasGenerated &&
            generatedId != before["obj"][1]["advancedKeyboard"]["trieId"] && imported["obj"][1]["advancedKeyboard"]["trieSizeKiB"] == 0,
            "Generated model was not embedded and assigned to the selected Advanced Keyboard");
    auto* predictor = lv_advanced_keyboard_get_predictor(keyboard);
    lv_advanced_keyboard_suggestion_t suggestions[3]{};
    Require(predictor && lv_advanced_keyboard_predictor_suggest_folded(predictor, "hel", 3, nullptr, 0, nullptr, 0, suggestions, 3) >= 1,
            "Imported generated model did not supply real suggestions");
    Click(window, "Load into project"); Require(editor.ProjectDocument() == imported, "Importing the same generated model duplicated or changed its project asset");
    LanguageModelWindow::Close(); Require(!LanguageModelWindow::ActiveWindow(), "Completed generator window did not close");
    Require(fs::is_regular_file(session / "language-model-settings.json"), "Generator settings were not saved in the isolated session");
    Require(editor.Undo(), "Loading the generated model was not undoable");
    Require(editor.ProjectDocument() == before, "Undo did not restore the original bundled model, size and project catalog");
    Require(editor.Redo() && editor.ProjectDocument() == imported, "Redo did not restore the embedded generated model");
    keyboard = lv_obj_get_child(surface, 1); properties->SetSelectedObject(keyboard);
    editor.MenuAction("language-model-generator"); window = LanguageModelWindow::ActiveWindow();
    Require(std::string(lv_textarea_get_text(Field(window, "Worker threads"))) == "2", "Generator settings did not reload");
    Click(window, "Generate trie");
    Require(!LanguageModelWindow::Running() && Label(lv_layer_top(), "Replace language model?"), "Existing model did not require replacement confirmation");
    auto* confirmTitle = Label(lv_layer_top(), "Replace language model?");
    Click(lv_obj_get_parent(lv_obj_get_parent(confirmTitle)), "Cancel");
    Require(!LanguageModelWindow::Running(), "Cancelling replacement started generation");
    std::string large; for (unsigned i = 0; i < 100000; ++i) large += "the world hello there help the hero\n";
    Write(work / "corpus/input.txt", large);
    Text(window, "Trie output file", (work / "cancelled.ngtr").u8string()); Click(window, "Generate trie"); Click(window, "Stop"); Wait(simulator);
    Require(!fs::exists(work / "cancelled.ngtr") && HasText(window, "Generation stopped."), "Stop did not cancel generation without publishing output");
    Text(window, "Trie output file", (work / "closed.ngtr").u8string()); Click(window, "Generate trie");
    auto* close = EditorWindow::CloseButton(window); Require(close, "Generator window close button missing"); lv_obj_send_event(close, LV_EVENT_CLICKED, nullptr); Wait(simulator, true);
    Require(!LanguageModelWindow::ActiveWindow() && !LanguageModelWindow::Running() && !fs::exists(work / "closed.ngtr"), "Closing a running generator left a job or partial output");
    editor.MenuAction("language-model-generator"); window = LanguageModelWindow::ActiveWindow();
    Text(window, "Trie output file", (work / "generated.ngtr").u8string());
    std::cout << "Top-level model menu, native LVGL generation, progress/cancel/close, model import, undo/redo, replacement protection and isolated settings passed\n";
}
