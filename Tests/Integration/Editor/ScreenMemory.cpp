#include "ScreenMemory.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include <src/core/lv_obj_private.h>
#include <src/core/lv_obj_class_private.h>
#include <src/widgets/button/lv_button_private.h>
#include <src/widgets/label/lv_label_private.h>
#include <src/widgets/textarea/lv_textarea_private.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

lv_obj_t* Label(lv_obj_t* root, const std::string& text, bool prefix = false)
{
    if (lv_obj_has_class(root, &lv_label_class)) {
        const std::string caption = lv_label_get_text(root);
        if (prefix ? caption.rfind(text, 0) == 0 : caption == text) return root;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* found = Label(lv_obj_get_child(root, i), text, prefix)) return found;
    return nullptr;
}

void Drain(SdlSimulator& simulator)
{
    // Refresh uses the existing 200 ms workspace timer, not a test-only hook.
    const auto deadline = SDL_GetTicks() + 260;
    do {
        Require(simulator.ProcessEvents(), "The editor stopped during the screen memory test");
    } while (SDL_GetTicks() < deadline);
    lv_obj_update_layout(lv_screen_active());
}

void ExpectBytes(lv_obj_t* badge, size_t bytes, const char* message)
{
    const auto expected = "Screen Memory: " + std::to_string(bytes) + " bytes";
    if (std::string(lv_label_get_text(badge)) != expected)
        throw std::runtime_error(std::string(message) + ": expected '" + expected +
            "', got '" + lv_label_get_text(badge) + "'");
}

bool Intersects(const lv_area_t& first, const lv_area_t& second)
{
    return first.x1 <= second.x2 && first.x2 >= second.x1 &&
        first.y1 <= second.y2 && first.y2 >= second.y1;
}

json TextRecord(int oid, const char* text)
{
    return {{"oid", oid}, {"par", -1}, {"text", text},
        {"base", {{"coords", {{"x", 12}, {"y", 12 + oid * 30}, {"width", 150}, {"height", 24}}}}}};
}

struct ExtendedWidget {
    lv_obj_t object;
    uint64_t payload[3];
};

lv_obj_t* DerivedWidget(lv_obj_t* parent, bool extended)
{
    static const lv_obj_class_t inherited = [] {
        lv_obj_class_t value{};
        value.base_class = &lv_obj_class;
        value.name = "screen_memory_inherited";
        return value;
    }();
    static const lv_obj_class_t sized = [] {
        auto value = inherited;
        value.instance_size = sizeof(ExtendedWidget);
        value.name = "screen_memory_extended";
        return value;
    }();
    auto* object = lv_obj_class_create_obj(extended ? &sized : &inherited, parent);
    Require(object, "The derived widget fixture could not be created");
    lv_obj_class_init_obj(object);
    return object;
}
}

void CheckScreenMemory(UI& editor, SdlSimulator& simulator)
{
    auto* title = Label(lv_screen_active(), "Simulator [Size:320x240]");
    Require(title, "The existing simulator title is missing");
    auto* frame = lv_obj_get_parent(lv_obj_get_parent(title));
    auto* header = EditorWindow::Header(frame);
    auto* surface = lv_obj_get_child(EditorWindow::Content(frame), 0);
    auto* badge = Label(header, "Screen Memory: ", true);
    Require(surface && badge && lv_obj_get_parent(badge) == header,
        "The screen memory total is missing from the window frame");
    Require(lv_obj_get_child_count(surface) == 0, "The memory fixture must start with an empty screen");
    ExpectBytes(badge, sizeof(lv_obj_t), "The initial screen object was not counted");

    Drain(simulator);
    lv_area_t titleArea, badgeArea, headerArea;
    lv_obj_get_coords(title, &titleArea);
    lv_obj_get_coords(badge, &badgeArea);
    lv_obj_get_coords(header, &headerArea);
    Require(badgeArea.x1 >= headerArea.x1 && badgeArea.x2 <= headerArea.x2 &&
        badgeArea.y1 >= headerArea.y1 && badgeArea.y2 <= headerArea.y2,
        "The screen memory badge extends outside its frame");
    Require(!Intersects(titleArea, badgeArea), "The memory badge overlaps the simulator title");
    bool settingsFound = false;
    for (uint32_t i = 0; i < lv_obj_get_child_count(header); ++i) {
        auto* child = lv_obj_get_child(header, i);
        if (!lv_obj_has_class(child, &lv_button_class)) continue;
        settingsFound = true;
        lv_area_t buttonArea;
        lv_obj_get_coords(child, &buttonArea);
        Require(!Intersects(buttonArea, badgeArea), "The memory badge overlaps a frame button");
    }
    Require(settingsFound, "The frame settings button is missing");

    auto* container = lv_obj_create(surface);
    auto* button = lv_button_create(container);
    auto* text = lv_label_create(button);
    lv_label_set_text(text, "Nested button label");
    auto* hidden = lv_label_create(container);
    lv_label_set_text(hidden, "Hidden objects still occupy memory");
    lv_obj_add_flag(hidden, LV_OBJ_FLAG_HIDDEN);
    auto* textarea = lv_textarea_create(container);
    Require(lv_obj_get_child_count(textarea) == 1, "The textarea fixture must own one internal label");
    auto* outside = lv_label_create(lv_layer_top());
    lv_label_set_text(outside, "Editor chrome is outside the screen");
    const size_t nestedBytes = 2 * sizeof(lv_obj_t) + sizeof(lv_button_t) +
        3 * sizeof(lv_label_t) + sizeof(lv_textarea_t);
    Drain(simulator);
    ExpectBytes(badge, nestedBytes, "Nested, hidden or internal widgets were counted incorrectly");

    lv_label_set_text(text, "Longer text changes its allocation, but not the LVGL widget instance size");
    Drain(simulator);
    ExpectBytes(badge, nestedBytes, "Text storage was included in the widget instance total");
    lv_obj_delete(outside);
    Drain(simulator);
    ExpectBytes(badge, nestedBytes, "An unrelated editor object changed the screen memory total");

    auto* inherited = DerivedWidget(container, false);
    auto* extended = DerivedWidget(container, true);
    Drain(simulator);
    ExpectBytes(badge, nestedBytes + sizeof(lv_obj_t) + sizeof(ExtendedWidget),
        "Inherited or extended widget classes did not match LVGL instance allocation sizes");
    lv_obj_delete(inherited);
    lv_obj_delete(extended);
    lv_obj_delete(container);
    Drain(simulator);
    ExpectBytes(badge, sizeof(lv_obj_t), "Deleting a widget subtree did not reduce the byte count");

    const json canvas = {{"width", 320}, {"height", 240}};
    const auto first = json::array({TextRecord(0, "First screen")});
    const auto second = json::array({TextRecord(0, "Second screen"), TextRecord(1, "Another label")});
    json document = {{"canvas", canvas}, {"obj", first}, {"variables", json::array()},
        {"activeScreen", 0}, {"editorScreen", 0}, {"screens", json::array({
            {{"id", "memory-first"}, {"name", "First"}, {"canvas", canvas}, {"obj", first}},
            {{"id", "memory-second"}, {"name", "Second"}, {"canvas", canvas}, {"obj", second}}
        })}};
    editor.ApplyDocument(document);
    Drain(simulator);
    ExpectBytes(badge, sizeof(lv_obj_t) + sizeof(lv_label_t), "Loading a document did not refresh the total");
    Require(editor.SelectScreen(1), "The second screen could not be selected");
    Drain(simulator);
    ExpectBytes(badge, sizeof(lv_obj_t) + 2 * sizeof(lv_label_t), "Switching screens retained the previous byte count");
    Require(editor.SelectScreen(0), "The first screen could not be selected again");
    Drain(simulator);
    ExpectBytes(badge, sizeof(lv_obj_t) + sizeof(lv_label_t), "Switching back did not refresh the byte count");

    std::cout << "Screen frame byte totals, nested/internal/hidden widgets, editor exclusion, deletion and screen switching passed\n";
}
