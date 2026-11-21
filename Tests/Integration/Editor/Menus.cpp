#include "Menus.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/LanguageModels/LanguageModelWindow.h"
#include <Serialization/ObjectSerializer.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
void Require(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
lv_obj_t* Label(lv_obj_t* root, const std::function<bool(const std::string&)>& matches) {
    if (lv_obj_has_class(root, &lv_label_class) && matches(lv_label_get_text(root))) return root;
    for (uint32_t index = 0; index < lv_obj_get_child_count(root); ++index)
        if (auto* found = Label(lv_obj_get_child(root, index), matches)) return found;
    return nullptr;
}
lv_obj_t* Label(lv_obj_t* root, const char* caption) {
    return Label(root, [caption](const std::string& text) { return text == caption; });
}
lv_obj_t* ButtonFor(lv_obj_t* label) {
    for (auto* object = label; object; object = lv_obj_get_parent(object))
        if (lv_obj_has_class(object, &lv_button_class)) return object;
    return nullptr;
}
lv_obj_t* Command(const char* prefix) {
    auto* label = Label(lv_layer_top(), [prefix](const std::string& text) {
        const std::string expected = prefix;
        return text == expected || (text.compare(0, expected.size(), expected) == 0 &&
            text.size() > expected.size() && (text[expected.size()] == '.' || text[expected.size()] == ' ' || text[expected.size()] == '\t'));
    });
    auto* button = ButtonFor(label);
    Require(button, std::string("Menu command missing: ") + prefix);
    return button;
}
void Activate(lv_obj_t* button) {
    Require(button && !lv_obj_has_state(button, LV_STATE_DISABLED), "Expected enabled menu action");
    lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(lv_layer_top());
}
void Click(const char* prefix) { Activate(Command(prefix)); }
void Drain(SdlSimulator& simulator) { for (int frame = 0; frame < 12; ++frame) simulator.ProcessEvents(); }
void QueueText(const char* text) {
    SDL_Event event{}; event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    event.text.text = text; Require(SDL_PushEvent(&event), "Cannot queue menu text-input event");
}
void QueueKey(SDL_Keycode key, SDL_Scancode scan) {
    SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    event.key.key = key; event.key.scancode = scan; event.key.down = true;
    Require(SDL_PushEvent(&event), "Cannot queue menu keyboard event");
}
lv_obj_t* HeaderButton(lv_obj_t* root, const char* caption) {
    if (lv_obj_has_class(root, &lv_label_class) && std::string(lv_label_get_text(root)) == caption) {
        if (auto* button = ButtonFor(root)) {
            lv_obj_update_layout(button); lv_area_t bounds{}; lv_obj_get_coords(button, &bounds);
            if (bounds.y1 >= 0 && bounds.y2 < 36) return button;
        }
    }
    for (uint32_t index = 0; index < lv_obj_get_child_count(root); ++index)
        if (auto* found = HeaderButton(lv_obj_get_child(root, index), caption)) return found;
    return nullptr;
}
void PointerClick(lv_obj_t* button, SdlSimulator& simulator) {
    Require(button, "Missing pointer target");
    lv_obj_update_layout(button);
    lv_area_t bounds{}; lv_obj_get_coords(button, &bounds);
    const float x = static_cast<float>((bounds.x1 + bounds.x2) / 2);
    const float y = static_cast<float>((bounds.y1 + bounds.y2) / 2);
    const auto id = SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION; motion.motion.windowID = id;
    motion.motion.x = x; motion.motion.y = y; SDL_PushEvent(&motion);
    SDL_Event event{}; event.type = SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.windowID = id;
    event.button.button = SDL_BUTTON_LEFT; event.button.down = true; event.button.x = x; event.button.y = y;
    SDL_PushEvent(&event);
    for (int frame = 0; frame < 8; ++frame) simulator.ProcessEvents();
    event.type = SDL_EVENT_MOUSE_BUTTON_UP; event.button.down = false; SDL_PushEvent(&event);
    for (int frame = 0; frame < 8; ++frame) simulator.ProcessEvents();
    lv_obj_update_layout(lv_layer_top());
}
void Menu(UI& editor, SdlSimulator& simulator, const char* caption, bool pointer = true) {
    if (pointer) PointerClick(HeaderButton(lv_screen_active(), caption), simulator);
    else editor.MenuAction(std::string("menu:") + caption);
    Require(lv_obj_get_child_count(lv_layer_top()) != 0, std::string("Menu did not open: ") + caption);
}
lv_obj_t* Window(lv_obj_t* layer, const char* caption) {
    auto* label = Label(layer, caption); Require(label, std::string("Window did not open: ") + caption);
    return lv_obj_get_parent(lv_obj_get_parent(label));
}
void CloseWindow(const char* caption) {
    auto* window = Window(lv_layer_top(), caption);
    auto* close = EditorWindow::CloseButton(window); Require(close, "Dialog has no close action");
    Activate(close);
    Require(!Label(lv_layer_top(), caption), std::string("Window did not close: ") + caption);
}
void Disabled(const char* caption, bool expected) {
    Require(lv_obj_has_state(Command(caption), LV_STATE_DISABLED) == expected,
        std::string("Incorrect action availability: ") + caption);
}
void PopupBounds() {
    auto* layer = lv_layer_top();
    Require(lv_obj_get_child_count(layer) != 0, "No menu popup to inspect");
    auto* popup = lv_obj_get_child(layer, -1); lv_obj_update_layout(popup);
    const auto width = lv_display_get_horizontal_resolution(nullptr), height = lv_display_get_vertical_resolution(nullptr);
    unsigned panels = 0;
    for (uint32_t index = 0; index < lv_obj_get_child_count(popup); ++index) {
        auto* panel = lv_obj_get_child(popup, index);
        if (lv_obj_is_hidden(panel)) continue;
        lv_area_t bounds{}; lv_obj_get_coords(panel, &bounds);
        Require(bounds.x1 >= 0 && bounds.x2 < width && bounds.y1 >= 36 && bounds.y2 < height,
            "Menu or flyout extends outside the visible display");
        ++panels;
    }
    Require(panels != 0, "Menu popup has no visible panels");
}
void Palette(UI& editor, const char* caption) {
    editor.MenuAction("dismiss"); editor.MenuAction("select-screen");
    auto* title = Label(lv_screen_active(), "Tool Tray"); Require(title, "Tool Tray is missing");
    auto* palette = EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title)));
    Activate(ButtonFor(Label(palette, caption)));
}
}

void CheckMenusEditor(UI& editor, SdlSimulator& simulator) {
    const auto work = fs::absolute("menu-editor-work") / ("run-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(work / "session");
#ifdef _WIN32
    Require(SetEnvironmentVariableW(L"UICREATOR_SESSION_DIR", (work / "session").c_str()) != 0, "Cannot isolate menu test settings");
#else
    Require(setenv("UICREATOR_SESSION_DIR", (work / "session").c_str(), 1) == 0, "Cannot isolate menu test settings");
#endif
    const auto original = editor.ProjectDocument();
    Require(!editor.Dirty() && original["obj"].empty(), "Menu fixture must begin with a clean document");

    Menu(editor, simulator, "File");
    Require(Command("Open project"), "File commands unavailable");
    Menu(editor, simulator, "Assets");
    Require(Command("Project fonts") && !Label(lv_layer_top(), "Open project..."),
        "Clicking another top-level menu did not switch the open popup");
    for (const auto& item : {std::make_pair("Project fonts", "Project fonts"), std::make_pair("Project images", "Project images"),
                           std::make_pair("Convert font", "Font converter"), std::make_pair("Convert images", "Image converter")}) {
        editor.MenuAction("dismiss"); Menu(editor, simulator, "Assets");
        Click(item.first); CloseWindow(item.second);
        Require(editor.ProjectDocument() == original, "Asset tool menus changed the authored screen");
    }
    editor.MenuAction("dismiss");
    Menu(editor, simulator, "File");
    Require(editor.HandleShortcut(SDLK_DOWN, false) && editor.HandleShortcut(SDLK_RETURN, false),
        "Menu arrow/Enter navigation was not handled");
    CloseWindow("Open project");
    Menu(editor, simulator, "File"); Click("Save project"); CloseWindow("Save project");

    for (const auto& item : {std::make_pair("Device Protobuf", "Export device project"),
                           std::make_pair("LVGL source project", "Export C project"),
                           std::make_pair("Create firmware project", "Create LVGL firmware project")}) {
        Menu(editor, simulator, "Export"); Click(item.first); CloseWindow(item.second);
    }
    Require(editor.HandleShortcut('l', false, false, true), "Alt+L did not open Language Models");
    Click("Generate language model"); Require(LanguageModelWindow::ActiveWindow(), "Model generator menu route failed");
    LanguageModelWindow::Close();
    Require(editor.ProjectDocument() == original && !editor.Dirty(), "Opening and closing tools dirtied the project");

    Menu(editor, simulator, "Edit");
    Disabled("Undo", true); Disabled("Redo", true); Disabled("Duplicate selection", true); Disabled("Delete selection", true);
    Disabled("Align", true); Disabled("Distribute", true); editor.MenuAction("dismiss");
    Menu(editor, simulator, "Debug"); Disabled("Stop debug viewer", true);
    Require(editor.HandleShortcut(SDLK_ESCAPE, false), "Escape did not dismiss Debug menu");
    Require(lv_obj_get_child_count(lv_layer_top()) == 0, "Escape left an open root popup");

    // Enough recent projects to exercise the bounded flyout on short displays.
    for (unsigned index = 0; index < 8; ++index) {
        const auto path = work / ("Menu example " + std::to_string(index + 1) + ".json");
        std::ofstream output(path, std::ios::binary); output << original.dump(); output.close();
        Require(bool(output), "Cannot write recent-project fixture");
        editor.MenuAction("recent:" + path.u8string());
    }
    Require(editor.RecentProjects().size() == 8, "Recent-project fixture did not populate the menu");
    const auto empty = editor.ProjectDocument();
    Menu(editor, simulator, "File"); Click("Recent projects");
    Require(editor.HandleShortcut(SDLK_ESCAPE, false) && Command("Open project"), "Escape did not return from the Recent projects flyout");
    Require(editor.HandleShortcut(SDLK_ESCAPE, false) && lv_obj_get_child_count(lv_layer_top()) == 0,
        "Second Escape did not close the root menu");
    Require(editor.ProjectDocument() == empty, "Recent-project navigation changed the screen");

    auto* preview = Window(lv_screen_active(), "Simulator [Size:320x240]");
    auto* tray = Window(lv_screen_active(), "Tool Tray");
    Activate(EditorWindow::CloseButton(tray)); Require(lv_obj_is_hidden(tray), "Panel did not hide");
    Menu(editor, simulator, "View"); Click("Show panel"); Click("Tool Tray");
    Require(!lv_obj_is_hidden(tray), "Show panel flyout did not restore the requested panel");
    Menu(editor, simulator, "View"); Click("Zoom in");
    Require(lv_obj_get_style_transform_scale_x(preview, LV_PART_MAIN) > 256, "View Zoom in did not zoom the canvas");
    Menu(editor, simulator, "View"); Click("Actual size");
    Require(lv_obj_get_style_transform_scale_x(preview, LV_PART_MAIN) == 256, "Actual size did not restore 100% zoom");
    Require(editor.ProjectDocument() == empty, "Panel or canvas view menus changed authored data");

    Palette(editor, "Bar");
    const auto single = editor.ProjectDocument(); Require(single["obj"].size() == 1, "Cannot create menu selection fixture");
    Menu(editor, simulator, "Edit"); Disabled("Duplicate selection", false); Disabled("Align", true); Disabled("Distribute", true);
    Click("Duplicate selection"); Require(editor.ProjectDocument()["obj"].size() == 2, "Duplicate menu command did not duplicate selection");
    Menu(editor, simulator, "Edit"); Disabled("Undo", false); Click("Undo");
    Require(editor.ProjectDocument()["obj"].size() == 1, "Undo menu command did not restore the selection edit");
    Palette(editor, "Bar"); Palette(editor, "Bar");
    auto* surface = lv_obj_get_child(EditorWindow::Content(preview), 0);
    auto* first = lv_obj_get_child(surface, 0); auto* second = lv_obj_get_child(surface, 1); auto* third = lv_obj_get_child(surface, 2);
    Require(first && second && third, "Cannot find selection fixture widgets");
    auto* properties = static_cast<ObjectUserData*>(lv_obj_get_user_data(first))->toolTray->GetPropertyWindow();
    lv_obj_set_pos(first, 10, 10); lv_obj_set_pos(second, 100, 60); lv_obj_set_pos(third, 220, 110);
    properties->SetSelection({first, second});
    Menu(editor, simulator, "Edit"); Disabled("Align", false); Disabled("Distribute", true); Click("Align"); Click("Left");
    Require(lv_obj_get_x(first) == lv_obj_get_x(second), "Align flyout did not align the selected siblings");
    properties->SetSelection({first, second, third});
    Menu(editor, simulator, "Edit"); Disabled("Distribute", false); Click("Distribute"); Click("Horizontally");
    const auto gap1 = lv_obj_get_x(second) - lv_obj_get_x(first), gap2 = lv_obj_get_x(third) - lv_obj_get_x(second);
    Require(std::abs(gap1 - gap2) <= 1, "Distribute flyout did not space the selected siblings evenly");

    Palette(editor, "Text area");
    auto* field = lv_obj_get_child(surface, -1);
    Require(lv_obj_has_class(field, &lv_textarea_class), "Cannot create menu text-input fixture");
    properties->SetSelectedObject(field); lv_textarea_set_text(field, "");
    QueueText("baseline"); Drain(simulator);
    Require(std::string(lv_textarea_get_text(field)) == "baseline", "Physical text input did not reach the selected text field before opening a menu");
    editor.MenuAction("zoom-in");
    Menu(editor, simulator, "View");
    QueueText("should-not-type"); Drain(simulator);
    Require(std::string(lv_textarea_get_text(field)) == "baseline", "Typing while a menu is open leaked into the selected text field");
    // One SDL event batch models Space's KEY_DOWN followed by its TEXT_INPUT.
    // The key activates Actual size and closes the popup before the text arrives.
    auto* actualSize=Command("Actual size");
    const auto visibleRows=lv_obj_get_child_count(lv_obj_get_parent(actualSize));
    for(uint32_t step=0;step<visibleRows&&!lv_obj_has_state(actualSize,LV_STATE_FOCUSED);++step)
        Require(editor.HandleShortcut(SDLK_DOWN,false),"Cannot navigate to the intended View command");
    Require(lv_obj_has_state(actualSize,LV_STATE_FOCUSED),"Keyboard menu navigation did not select Actual size");
    QueueKey(SDLK_SPACE, SDL_SCANCODE_SPACE); QueueText(" "); Drain(simulator);
    Require(lv_obj_get_child_count(lv_layer_top()) == 0 && lv_obj_get_style_transform_scale_x(preview, LV_PART_MAIN) == 256,
        "Space did not activate the keyboard-selected menu action");
    Require(std::string(lv_textarea_get_text(field)) == "baseline", "The queued Space text leaked into the text field after its menu action closed the popup");
    QueueKey(static_cast<SDL_Keycode>('b'), SDL_SCANCODE_B); QueueText("b"); Drain(simulator);
    Require(std::string(lv_textarea_get_text(field)) == "baselineb", "Closing a menu swallowed subsequent normal text input");
    properties->SetSelection({first, second, third});

    const auto laidOut = editor.ProjectDocument();
    auto* display = lv_display_get_default();
    const auto width = lv_display_get_horizontal_resolution(display), height = lv_display_get_vertical_resolution(display);
    lv_display_set_resolution(display, 800, 320); lv_obj_update_layout(lv_screen_active());
    for (const auto* name : {"File", "Edit", "View", "Assets", "Language Models", "Export", "Debug", "Help"}) {
        editor.MenuAction(std::string("menu:") + name); PopupBounds(); editor.MenuAction("dismiss");
    }
    editor.MenuAction("menu:File"); Click("Recent projects"); PopupBounds();
    const auto oldest = fs::u8path(editor.RecentProjects().back()).filename().u8string();
    auto* oldestLabel = Label(lv_layer_top(), [&oldest](const std::string& text) { return text.find(oldest) != std::string::npos; });
    auto* oldestButton = ButtonFor(oldestLabel); Require(oldestButton, "Recent projects flyout lost its oldest entry");
    lv_obj_scroll_to_view(oldestButton, LV_ANIM_OFF); lv_obj_update_layout(lv_layer_top()); PopupBounds();
    lv_area_t row{}, panel{}; lv_obj_get_coords(oldestButton, &row); lv_obj_get_coords(lv_obj_get_parent(oldestButton), &panel);
    Require(row.y1 >= panel.y1 && row.y2 <= panel.y2, "Last recent project cannot be scrolled into view");
    editor.MenuAction("dismiss");
    lv_display_set_resolution(display, width, height); lv_obj_update_layout(lv_screen_active());
    Require(editor.ProjectDocument() == laidOut, "Menu resizing/navigation modified the document");
    Menu(editor, simulator, "Edit"); Click("Align");
    std::cout << "Menu routing, top-bar switching, keyboard/text isolation, selection eligibility, canvas/panel actions, bounded flyouts and document stability passed\n";
}
