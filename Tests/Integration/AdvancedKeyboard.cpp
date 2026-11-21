#include <Serialization/AdvancedKeyboard.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Project.h>
#include "../../CodeGeneration/CCodeGenerator.h"
#include "../../Runtime/include/lvgl_runtime.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Serialization;
namespace
{
void Check(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}
unsigned flushes = 0;
lv_indev_t* pointer = nullptr;
lv_point_t pointerPoint{};
lv_indev_state_t pointerState = LV_INDEV_STATE_RELEASED;
void ReadPointer(lv_indev_t*, lv_indev_data_t* data)
{
    data->point = pointerPoint;
    data->state = pointerState;
}
void Flush(lv_display_t* display, const lv_area_t*, uint8_t*)
{
    ++flushes;
    lv_display_flush_ready(display);
}
void Advance(uint32_t milliseconds)
{
    for (uint32_t elapsed = 0; elapsed < milliseconds; elapsed += 20) {
        lv_tick_inc((std::min)(20u, milliseconds - elapsed));
        lv_timer_handler();
    }
}
lv_obj_t* Button(lv_obj_t* container, const char* caption)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(container); ++i) {
        auto* child = lv_obj_get_child(container, static_cast<int32_t>(i));
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN) || !lv_obj_has_class(child, &lv_button_class)) continue;
        for (uint32_t j = 0; j < lv_obj_get_child_count(child); ++j) {
            auto* label = lv_obj_get_child(child, static_cast<int32_t>(j));
            if (lv_obj_has_class(label, &lv_label_class) && std::strcmp(lv_label_get_text(label), caption) == 0)
                return child;
        }
    }
    return nullptr;
}
void Click(lv_obj_t* button)
{
    lv_obj_update_layout(button);
    lv_area_t bounds{}; lv_obj_get_coords(button, &bounds);
    pointerPoint = {static_cast<int32_t>((bounds.x1 + bounds.x2) / 2), static_cast<int32_t>((bounds.y1 + bounds.y2) / 2)};
    pointerState = LV_INDEV_STATE_RELEASED; lv_indev_read(pointer);
    pointerState = LV_INDEV_STATE_PRESSED; lv_indev_read(pointer);
    pointerState = LV_INDEV_STATE_RELEASED; lv_indev_read(pointer);
}
void Tap(lv_obj_t* keyboard, const char* caption)
{
    auto* button = Button(keyboard, caption);
    Check(button && !lv_obj_has_state(button, LV_STATE_DISABLED), std::string("Missing active key: ") + caption);
    Click(button);
}
void Text(lv_obj_t* object, const std::string& expected)
{
    const std::string actual = lv_advanced_keyboard_get_text(object);
    Check(actual == expected, "Typing mismatch: expected '" + expected + "', got '" + actual + "'");
}
struct LookupState
{
    unsigned calls = 0;
    unsigned malformed = 0;
    std::string prefix, previous, older;
};
size_t Lookup(void* context, const char* prefix, size_t prefixLength,
              const char* previous, size_t previousLength, const char* older, size_t olderLength,
              lv_advanced_keyboard_suggestion_t* output, size_t capacity)
{
    auto& state = *static_cast<LookupState*>(context);
    ++state.calls;
    state.prefix.assign(prefixLength ? prefix : "", prefixLength);
    state.previous.assign(previousLength ? previous : "", previousLength);
    state.older.assign(olderLength ? older : "", olderLength);
    if (state.prefix != "ca" || !capacity) return 0;
    if (state.malformed) {
        if (state.malformed == 1) output[0].length = LV_ADVANCED_KEYBOARD_SUGGESTION_BYTES + 1;
        else if (state.malformed == 2) { std::memcpy(output[0].text, "c\0a", 3); output[0].length = 3; }
        else if (state.malformed == 3) { std::memcpy(output[0].text, "\xe2\x82", 2); output[0].length = 2; }
        else { std::memcpy(output[0].text, "\xc0\xaf", 2); output[0].length = 2; }
        return 1;
    }
    const char word[] = "caf\xc3\xa9";
    std::memcpy(output[0].text, word, sizeof word);
    output[0].length = sizeof word - 1;
    output[0].mass = 100;
    return 1;
}
void CountEdits(lv_event_t* event)
{
    ++*static_cast<unsigned*>(lv_event_get_user_data(event));
}
void RefocusReady(lv_event_t* event)
{
    lv_obj_send_event(static_cast<lv_obj_t*>(lv_event_get_user_data(event)), LV_EVENT_FOCUSED, nullptr);
}
struct DeleteOnReady { lv_obj_t* keyboard; bool cleared = false; };
void DeleteKeyboardReady(lv_event_t* event)
{
    auto& state = *static_cast<DeleteOnReady*>(lv_event_get_user_data(event));
    state.cleared = lv_obj_has_flag(state.keyboard, LV_OBJ_FLAG_HIDDEN) && !lv_advanced_keyboard_get_textarea(state.keyboard);
    lv_obj_delete(state.keyboard); state.keyboard = nullptr;
}
void TargetAboveKeyboard(lv_obj_t* field, lv_obj_t* keyboard, const char* scenario)
{
    lv_obj_update_layout(field);
    lv_area_t target{}, keys{};
    lv_obj_get_coords(field, &target); lv_obj_get_coords(keyboard, &keys);
    Check(target.y1 >= 0 && target.y2 < keys.y1, std::string(scenario) + ": active field is clipped or covered by the keyboard");
}
json AuthoredView(lv_obj_t* object)
{
    const auto base = lv_advanced_keyboard_is_instance(object)
        ? ObjectSerializer::SerializeObject(object).at("base") : LVObject::ToJSON(object);
    return {{"coords", base.at("coords")}, {"flags", base.at("flags")},
            {"nativeStyles", base.at("nativeStyles")}, {"scroll", base.at("scroll")}};
}
lv_obj_t* RevealScreen()
{
    auto* screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen); lv_obj_set_size(screen, 640, 480);
    lv_obj_set_scroll_dir(screen, LV_DIR_VER);
    lv_screen_load(screen);
    return screen;
}
lv_obj_t* RevealKeyboard(lv_obj_t* parent)
{
    auto* keyboard = ObjectSerializer::Create("advancedKeyboard", parent);
    Check(keyboard, "Cannot create reveal fixture keyboard");
    lv_obj_set_pos(keyboard, 0, 260); lv_obj_set_size(keyboard, 640, 220);
    return keyboard;
}
void TestTargetReveal()
{
    auto* previous = lv_screen_active();
    auto* screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, 640, 480);
    lv_screen_load(screen);
    auto* form = lv_obj_create(screen);
    lv_obj_remove_style_all(form);
    lv_obj_set_size(form, 640, 480);
    lv_obj_set_scroll_dir(form, LV_DIR_VER);
    auto* extent = lv_obj_create(form);
    lv_obj_remove_style_all(extent);
    lv_obj_set_pos(extent, 0, 1000); lv_obj_set_size(extent, 1, 1);
    auto* first = lv_textarea_create(form);
    auto* last = lv_textarea_create(form);
    auto* visible = lv_textarea_create(form);
    for (auto* field : {first, last, visible}) lv_obj_set_size(field, 300, 60);
    lv_obj_set_pos(first, 20, 340); lv_textarea_set_text(first, "First retained text");
    lv_obj_set_pos(last, 20, 700); lv_textarea_set_text(last, "Last retained text");
    lv_obj_set_pos(visible, 20, 40); lv_textarea_set_text(visible, "Visible retained text");
    auto* keyboard = lv_advanced_keyboard_create(screen);
    Check(keyboard, "Cannot create target reveal keyboard");
    lv_obj_set_pos(keyboard, 0, 260); lv_obj_set_size(keyboard, 640, 220);
    for (auto* field : {first, last, visible}) Check(lv_advanced_keyboard_bind_textarea(keyboard, field), "Cannot bind target reveal field");
    lv_obj_update_layout(screen);
    lv_obj_send_event(visible, LV_EVENT_FOCUSED, nullptr); Advance(500);
    Check(lv_obj_get_scroll_y(form) == 0, "Opening keyboard needlessly scrolled an already visible target");
    lv_advanced_keyboard_close(keyboard);
    Click(first); Advance(500);
    TargetAboveKeyboard(first, keyboard, "Opening over lower field");
    Check(lv_advanced_keyboard_get_textarea(keyboard) == first && lv_obj_get_scroll_y(form) > 0 &&
          lv_obj_get_y(first) == 340 && lv_obj_get_y(keyboard) == 260,
          "Reveal moved authored geometry instead of scrolling the form");
    const auto firstScroll = lv_obj_get_scroll_y(form);
    lv_obj_send_event(last, LV_EVENT_FOCUSED, nullptr); Advance(500);
    TargetAboveKeyboard(last, keyboard, "Switching to a later field");
    Check(lv_obj_get_scroll_y(form) > firstScroll && lv_advanced_keyboard_get_textarea(keyboard) == last,
          "Target switch did not reveal and select the later field");
    lv_obj_send_event(visible, LV_EVENT_FOCUSED, nullptr); Advance(500);
    TargetAboveKeyboard(visible, keyboard, "Switching back to an earlier field");
    const auto stableScroll = lv_obj_get_scroll_y(form);
    lv_obj_send_event(visible, LV_EVENT_CLICKED, nullptr); Advance(500);
    Check(lv_obj_get_scroll_y(form) == stableScroll, "Clicking an already revealed target churned form scroll");
    Check(std::strcmp(lv_textarea_get_text(first), "First retained text") == 0 &&
          std::strcmp(lv_textarea_get_text(last), "Last retained text") == 0 &&
          std::strcmp(lv_textarea_get_text(visible), "Visible retained text") == 0,
          "Automatic reveal changed field contents");
    lv_advanced_keyboard_close(keyboard);
    Check(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN), "Reveal prevented keyboard dismissal");
    lv_screen_load(previous);
    lv_obj_delete(screen);
}
void TestSharedAncestorReveal()
{
    auto* previous = lv_screen_active();
    auto* screen = RevealScreen();
    auto* field = lv_textarea_create(screen);
    lv_obj_set_pos(field, 20, 400); lv_obj_set_size(field, 300, 60);
    lv_textarea_set_text(field, "Shared parent retained text");
    auto* keyboard = RevealKeyboard(screen);
    Check(lv_advanced_keyboard_bind_textarea(keyboard, field), "Cannot bind same-parent target");
    lv_obj_update_layout(screen);
    const auto beforeScreen = AuthoredView(screen), beforeField = AuthoredView(field), beforeKeyboard = AuthoredView(keyboard);
    const auto parentEvents = lv_obj_get_event_count(screen), children = lv_obj_get_child_count(screen);
    const auto originalBottom = lv_obj_get_scroll_bottom(screen);
    Check(originalBottom <= 0, "Same-parent reveal fixture unexpectedly has natural overflow");
    lv_obj_send_event(field, LV_EVENT_FOCUSED, nullptr); Advance(500);
    TargetAboveKeyboard(field, keyboard, "Same-parent field without natural overflow");
    lv_area_t keys{}; lv_obj_get_coords(keyboard, &keys);
    Check(keys.y1 == 260 && keys.y2 == 479 && lv_obj_get_scroll_y(screen) > 0,
          "Scrolling the shared ancestor moved the keyboard out of its authored viewport");
    Check(lv_obj_get_child_count(screen) == children, "Reveal inserted an authored child into the screen");
    Check(AuthoredView(screen) == beforeScreen && AuthoredView(field) == beforeField && AuthoredView(keyboard) == beforeKeyboard,
          "Active reveal leaked transient scrolling, pinning, styles or geometry into serialization");
    lv_advanced_keyboard_close(keyboard); Advance(20);
    Check(AuthoredView(screen) == beforeScreen && AuthoredView(field) == beforeField && AuthoredView(keyboard) == beforeKeyboard,
          "Dismissal did not restore authored viewport state");
    Check(lv_obj_get_scroll_y(screen) == 0 && lv_obj_get_scroll_bottom(screen) == originalBottom && lv_obj_get_event_count(screen) == parentEvents,
          "Dismissal left a transient scroll reservation or event hook");
    lv_obj_send_event(field, LV_EVENT_FOCUSED, nullptr); Advance(500);
    lv_obj_delete(keyboard); Advance(20);
    Check(AuthoredView(screen) == beforeScreen && AuthoredView(field) == beforeField && lv_obj_get_event_count(screen) == parentEvents,
          "Keyboard deletion left transient ancestor state or hooks");
    Check(std::strcmp(lv_textarea_get_text(field), "Shared parent retained text") == 0, "Reveal/deletion changed field content");
    lv_screen_load(previous); lv_obj_delete(screen);
}
void TestNestedViewportReveal()
{
    auto* previous = lv_screen_active();
    auto* screen = RevealScreen();
    auto* outer = lv_obj_create(screen); lv_obj_remove_style_all(outer);
    lv_obj_set_size(outer, 640, 480); lv_obj_set_scroll_dir(outer, LV_DIR_VER);
    auto* extent = lv_obj_create(outer); lv_obj_remove_style_all(extent);
    lv_obj_set_pos(extent, 0, 1000); lv_obj_set_size(extent, 1, 1);
    auto* inner = lv_obj_create(outer); lv_obj_remove_style_all(inner);
    lv_obj_set_pos(inner, 20, 320); lv_obj_set_size(inner, 600, 300); lv_obj_set_scroll_dir(inner, LV_DIR_VER);
    auto* innerExtent = lv_obj_create(inner); lv_obj_remove_style_all(innerExtent);
    lv_obj_set_pos(innerExtent, 0, 800); lv_obj_set_size(innerExtent, 1, 1);
    auto* field = lv_textarea_create(inner);
    lv_obj_set_pos(field, 20, 400); lv_obj_set_size(field, 300, 60); lv_textarea_set_text(field, "Nested retained text");
    auto* keyboard = RevealKeyboard(screen);
    Check(lv_advanced_keyboard_bind_textarea(keyboard, field), "Cannot bind nested reveal target");
    lv_obj_update_layout(screen);
    const auto beforeOuter = AuthoredView(outer), beforeInner = AuthoredView(inner), beforeKeyboard = AuthoredView(keyboard);
    const auto outerEvents = lv_obj_get_event_count(outer), innerEvents = lv_obj_get_event_count(inner);
    lv_obj_send_event(field, LV_EVENT_FOCUSED, nullptr); Advance(500);
    TargetAboveKeyboard(field, keyboard, "Nested scroll viewports");
    lv_area_t target{}, innerArea{}, outerArea{};
    lv_obj_get_coords(field, &target); lv_obj_get_content_coords(inner, &innerArea); lv_obj_get_content_coords(outer, &outerArea);
    Check(target.y1 >= (std::max)(innerArea.y1, outerArea.y1) && target.y2 <= (std::min)(innerArea.y2, outerArea.y2),
          "Reveal placed the field above the keyboard but outside an ancestor's clip");
    Check(lv_obj_get_scroll_y(inner) > 0 && lv_obj_get_scroll_y(outer) > 0,
          "Nested reveal did not scroll both clipping ancestors");
    Check(AuthoredView(outer) == beforeOuter && AuthoredView(inner) == beforeInner && AuthoredView(keyboard) == beforeKeyboard,
          "Nested reveal leaked temporary viewport state into serialization");
    lv_advanced_keyboard_close(keyboard); Advance(20);
    Check(lv_obj_get_event_count(outer) == outerEvents && lv_obj_get_event_count(inner) == innerEvents &&
          lv_obj_get_scroll_y(outer) == 0 && lv_obj_get_scroll_y(inner) == 0 &&
          AuthoredView(outer) == beforeOuter && AuthoredView(inner) == beforeInner,
          "Nested dismissal left transient scroll offsets or ancestor hooks");
    lv_obj_send_event(field, LV_EVENT_FOCUSED, nullptr); Advance(500);
    lv_obj_delete(outer); Advance(20);
    Check(lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN) && !lv_advanced_keyboard_get_textarea(keyboard) &&
          lv_advanced_keyboard_bound_textarea_count(keyboard) == 0 && AuthoredView(keyboard) == beforeKeyboard,
          "Deleting the target's parent left a visible keyboard, dangling bindings or pinning");
    auto* replacement = lv_obj_create(screen); lv_obj_remove_style_all(replacement); lv_obj_set_size(replacement, 640, 480);
    lv_obj_update_layout(replacement);
    Check(lv_obj_get_scroll_y(replacement) == 0 && lv_obj_get_scroll_bottom(replacement) <= 0,
          "Deleted ancestor's viewport reservation leaked into a replacement object");
    lv_screen_load(previous); lv_obj_delete(screen);
}
struct DeleteDuringReveal { lv_obj_t* keyboard = nullptr; bool armed = false; unsigned calls = 0; uint32_t minimumEvents = 0; };
void DeleteKeyboardDuringReveal(lv_event_t* event)
{
    auto& state = *static_cast<DeleteDuringReveal*>(lv_event_get_user_data(event));
    // Ignore LVGL's initial focus scroll; delete only after the keyboard has
    // opened and installed its transient viewport callback on this ancestor.
    if (!state.armed || !state.keyboard || lv_obj_has_flag(state.keyboard, LV_OBJ_FLAG_HIDDEN) ||
        lv_obj_get_event_count(lv_event_get_target_obj(event)) < state.minimumEvents) return;
    state.armed = false; ++state.calls;
    auto* keyboard = state.keyboard; state.keyboard = nullptr;
    lv_obj_delete(keyboard);
}
void TestRevealCallbackDeletion(lv_event_code_t code)
{
    auto* previous = lv_screen_active();
    auto* screen = RevealScreen();
    auto* form = lv_obj_create(screen); lv_obj_remove_style_all(form);
    lv_obj_set_size(form, 640, 480); lv_obj_set_scroll_dir(form, LV_DIR_VER);
    auto* field = lv_textarea_create(form);
    lv_obj_set_pos(field, 20, 400); lv_obj_set_size(field, 300, 60); lv_textarea_set_text(field, "Callback retained text");
    auto* keyboard = RevealKeyboard(screen);
    Check(lv_advanced_keyboard_bind_textarea(keyboard, field), "Cannot bind callback deletion fixture");
    lv_obj_update_layout(screen);
    const auto original = AuthoredView(form);
    DeleteDuringReveal deletion{keyboard};
    lv_obj_add_event_cb(form, DeleteKeyboardDuringReveal, code, &deletion);
    const auto events = lv_obj_get_event_count(form);
    deletion.minimumEvents = events + 1;
    deletion.armed = true;
    lv_obj_send_event(field, LV_EVENT_FOCUSED, nullptr); Advance(500);
    Check(deletion.calls == 1 && !deletion.keyboard, "Reveal did not exercise the keyboard-deleting ancestor callback");
    Check(AuthoredView(form) == original && lv_obj_get_event_count(form) == events,
          "Callback deletion left temporary viewport state or a dangling event hook");
    Check(std::strcmp(lv_textarea_get_text(field), "Callback retained text") == 0,
          "Callback deletion altered external field text");
    lv_obj_remove_event_cb_with_user_data(form, DeleteKeyboardDuringReveal, &deletion);
    lv_screen_load(previous); lv_obj_delete(screen);
}
void TestKeyboardLanguages()
{
    constexpr const char* ids[] = {"en-US", "es-ES", "fr-FR", "de-DE", "it-IT", "pt-PT"};
    Check(lv_advanced_keyboard_language_count() == 6, "Keyboard language catalog lost one of the six supported layouts");
    for (size_t i = 0; i < 6; ++i) {
        const auto* id = lv_advanced_keyboard_language_id(i);
        const auto* name = lv_advanced_keyboard_language_name(i);
        Check(id && std::strcmp(id, ids[i]) == 0 && name && *name, "Language catalog has an invalid stable ID or label");
    }
    auto* previous = lv_screen_active();
    auto* screen = RevealScreen();
    auto* field = lv_textarea_create(screen);
    lv_obj_set_pos(field, 20, 20); lv_obj_set_size(field, 500, 60);
    lv_textarea_set_text(field, "Retained ");
    auto* keyboard = RevealKeyboard(screen);
    auto* keys = lv_advanced_keyboard_get_keyboard(keyboard);
    lv_advanced_keyboard_set_autocorrect(keyboard, false);
    Check(lv_advanced_keyboard_set_textarea(keyboard, field), "Cannot attach multilingual target");
    auto setLanguage = [&](const char* id) {
        const std::string retained = lv_textarea_get_text(field);
        Check(lv_advanced_keyboard_set_language(keyboard, id) && std::strcmp(lv_advanced_keyboard_get_language(keyboard), id) == 0,
              std::string("Cannot select keyboard language ") + id);
        Check(lv_advanced_keyboard_get_textarea(keyboard) == field && !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN) &&
              std::string(lv_textarea_get_text(field)) == retained, "Changing keyboard language lost its target or edited its text");
        Check(ObjectSerializer::SerializeObject(keyboard).at("advancedKeyboard").at("language") == id,
              "Native language choice did not serialize");
    };
    setLanguage("es-ES");
    Tap(keys, u8"ñ"); Tap(keys, u8"⇧"); Advance(320); Tap(keys, u8"Ñ"); Tap(keys, "a");
    Text(keyboard, u8"Retained ñÑa");
    Check(!lv_advanced_keyboard_set_language(keyboard, "unknown") && !lv_advanced_keyboard_set_language(keyboard, nullptr) &&
          std::strcmp(lv_advanced_keyboard_get_language(keyboard), "es-ES") == 0 && Button(keys, u8"ñ"),
          "Rejected language changed the active layout");
    setLanguage("fr-FR");
    auto* frenchA = Button(keys, "a"); auto* frenchQ = Button(keys, "q");
    Check(frenchA && frenchQ && lv_obj_get_y(frenchA) < lv_obj_get_y(frenchQ), "French keyboard is missing its AZERTY placement");
    Tap(keys, u8"éàç"); Tap(keys, u8"œ");
    Text(keyboard, u8"Retained ñÑaœ");
    setLanguage("de-DE");
    auto* germanZ = Button(keys, "z"); auto* germanY = Button(keys, "y");
    Check(germanZ && germanY && lv_obj_get_y(germanZ) < lv_obj_get_y(germanY), "German keyboard is missing its QWERTZ placement");
    Tap(keys, u8"ü"); Tap(keys, u8"⇧"); Advance(320); Tap(keys, u8"Ü"); Tap(keys, u8"ß");
    Text(keyboard, u8"Retained ñÑaœüÜß");
    setLanguage("it-IT"); Tap(keys, u8"àèé"); Tap(keys, u8"è");
    Text(keyboard, u8"Retained ñÑaœüÜßè");
    setLanguage("pt-PT"); Tap(keys, u8"áâã"); Tap(keys, u8"ã");
    Text(keyboard, u8"Retained ñÑaœüÜßèã");
    setLanguage("en-US");
    Check(Button(keys, "q") && Button(keys, "space") && !Button(keys, u8"ã"), "English selection did not reset the accent page");
    Tap(keys, "q"); Text(keyboard, u8"Retained ñÑaœüÜßèãq");
    const auto* font = lv_obj_get_style_text_font(Button(keys, "q"), LV_PART_MAIN);
    for (uint32_t codepoint : {0x00e0u, 0x00e3u, 0x00e7u, 0x00e8u, 0x00e9u, 0x00d1u, 0x00f1u,
                              0x00dcu, 0x00fcu, 0x00dfu, 0x0152u, 0x0153u, 0x1e9eu}) {
        lv_font_glyph_dsc_t glyph{};
        Check(font && font->get_glyph_dsc && font->get_glyph_dsc(font, &glyph, codepoint, 0) && glyph.box_w > 0 && glyph.box_h > 0,
              "Bundled keyboard font is missing a real multilingual glyph");
    }
    lv_screen_load(previous); lv_obj_delete(screen);
}
}

int main(int argc, char** argv)
{
    try {
        Check(argc == 2, "Expected Advanced Keyboard fixture output directory");
        const auto directory = std::filesystem::u8path(argv[1]);
        std::filesystem::create_directories(directory);
        lv_init();
        auto* display = lv_display_create(640, 480);
        Check(display, "No display");
        std::vector<uint8_t> pixels(640 * 20 * 4);
        lv_display_set_buffers(display, pixels.data(), nullptr, static_cast<uint32_t>(pixels.size()), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, Flush);
        pointer = lv_indev_create(); Check(pointer, "No pointer input");
        lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(pointer, display);
        lv_indev_set_read_cb(pointer, ReadPointer);
        auto* previous = lv_screen_active();
        auto* object = ObjectSerializer::Create("advancedKeyboard", previous);
        Check(object && AdvancedKeyboard::Type(object) && lv_advanced_keyboard_is_instance(object), "Advanced Keyboard factory/type failed");
        Check(std::string(ObjectSerializer::TypeName(object)) == "advancedKeyboard", "Registry lost Advanced Keyboard type");
        auto* keyboard = lv_advanced_keyboard_get_keyboard(object);
        Check(keyboard && !lv_advanced_keyboard_get_textarea(object) && lv_obj_get_child_count(object) == 2, "Keyboard must not own a textarea");
        auto* editor = ObjectSerializer::Create("textarea", previous);
        auto* second = ObjectSerializer::Create("textarea", previous);
        auto* inspector = ObjectSerializer::Create("textarea", previous);
        lv_obj_set_pos(editor, 20, 10); lv_obj_set_size(editor, 280, 60); lv_textarea_set_text(editor, "");
        lv_obj_set_pos(second, 320, 10); lv_obj_set_size(second, 280, 60); lv_textarea_set_text(second, "Second field");
        lv_obj_set_pos(inspector, 20, 80); lv_obj_set_size(inspector, 280, 40); lv_textarea_set_text(inspector, "Inspector");
        lv_obj_set_pos(object, 20, 130);
        AdvancedKeyboard::BindTargets(previous, std::map<int, lv_obj_t*>{{7, object}, {8, editor}, {9, second}});
        Check(lv_advanced_keyboard_bound_textarea_count(object) == 2 && lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN), "Unfocused keyboard should be hidden and bound only to authored fields");
        const auto unfocused = ObjectSerializer::SerializeObject(object);
        Click(editor);
        Check(lv_advanced_keyboard_get_textarea(object) == editor && !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN), "Focusing an external field did not activate keyboard");
        Check(ObjectSerializer::SerializeObject(object) == unfocused, "Focus changed authored keyboard data");
        lv_advanced_keyboard_set_autocorrect(object, false);
        lv_obj_update_layout(object);
        unsigned edits = 0, ready = 0, fieldReady = 0, cancelled = 0, fieldCancelled = 0;
        lv_obj_add_event_cb(object, CountEdits, LV_EVENT_VALUE_CHANGED, &edits);
        lv_obj_add_event_cb(object, CountEdits, LV_EVENT_READY, &ready);
        lv_obj_add_event_cb(object, CountEdits, LV_EVENT_CANCEL, &cancelled);
        lv_obj_add_event_cb(editor, CountEdits, LV_EVENT_READY, &fieldReady);
        lv_obj_add_event_cb(editor, CountEdits, LV_EVENT_CANCEL, &fieldCancelled);
        Tap(keyboard, "h"); Tap(keyboard, "i");
        Tap(keyboard, "\xe2\x87\xa7"); Tap(keyboard, "a"); Tap(keyboard, "b");
        Text(object, "hiAb");
        Tap(keyboard, "\xe2\x87\xa7"); Tap(keyboard, "\xe2\x87\xa7");
        Tap(keyboard, "C"); Tap(keyboard, "D");
        Text(object, "hiAbCD");
        Tap(keyboard, "\xe2\x87\xaa"); Advance(320);
        Tap(keyboard, "e");
        Tap(keyboard, "#@!"); Tap(keyboard, "1"); Tap(keyboard, "@");
        Tap(keyboard, "=\\<"); Tap(keyboard, "\xe2\x82\xac");
        Text(object, "hiAbCDe1@\xe2\x82\xac");
        Tap(keyboard, "\xe2\x8c\xab");
        Text(object, "hiAbCDe1@");
        Tap(keyboard, "\xe2\x86\xb5");
        Check(lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) && !lv_advanced_keyboard_get_textarea(object) && ready == 1 && fieldReady == 1,
              "Enter did not dismiss and emit READY on field and keyboard");
        Check(std::string(lv_textarea_get_text(editor)) == "hiAbCDe1@", "Enter inserted an unwanted newline");
        Click(editor); lv_textarea_set_cursor_pos(editor, LV_TEXTAREA_CURSOR_LAST);
        Tap(keyboard, "ABC"); Tap(keyboard, "z");
        Text(object, "hiAbCDe1@z");
        Check(edits >= 12, "Actual key edits were not forwarded through root VALUE_CHANGED");

        AdvancedKeyboard::Apply(object, {{"keyBackground", 0xff123456u}, {"keyForeground", 0xffedcba9u}, {"keyRadius", 9}});
        auto* key = Button(keyboard, "a"); Check(key, "Missing styled key");
        Check(lv_color_to_int(lv_obj_get_style_bg_color(key, LV_PART_MAIN)) == 0x123456u, "Key palette was not applied");
        lv_obj_add_state(key, LV_STATE_PRESSED); Advance(250);
        Check(lv_color_to_int(lv_obj_get_style_bg_color(key, LV_PART_MAIN)) == 0xedcba9u, "Pressed key did not invert its palette");
        lv_obj_remove_state(key, LV_STATE_PRESSED); Advance(250);
        Text(object, "hiAbCDe1@z");
        const auto oldKeyboardWidth = lv_obj_get_width(keyboard);
        lv_obj_set_size(object, 480, 280); lv_obj_update_layout(object);
        Check(lv_obj_get_width(keyboard) < oldKeyboardWidth && lv_obj_get_width(keyboard) > 400, "Mobile keys did not follow typing view resize");
        Check(lv_obj_get_width(Button(keyboard, "a")) > 0, "Resize collapsed active keys");

        LookupState lookup;
        lv_advanced_keyboard_set_suggestion_lookup(object, Lookup, &lookup);
        lv_advanced_keyboard_set_text(object, "old word Ca");
        lv_textarea_set_cursor_pos(editor, LV_TEXTAREA_CURSOR_LAST);
        Advance(80);
        Check(lookup.calls && lookup.prefix == "ca" && lookup.previous == "word" && lookup.older == "old", "Prediction callback did not receive folded text context");
        auto* suggestions = lv_obj_get_child(object, 0);
        Check(lv_obj_get_child_count(suggestions) == 4 && lv_obj_get_width(lv_obj_get_child(suggestions, 3)) >= 32 && lv_obj_get_height(lv_obj_get_child(suggestions, 3)) >= 32,
              "Close control displaced suggestion slots or has an undersized touch area");
        for (unsigned malformed = 1; malformed <= 4; ++malformed) {
            lookup.malformed = malformed;
            lv_advanced_keyboard_set_text(object, "");
            lv_advanced_keyboard_set_text(object, "ca"); Advance(80);
            for (uint32_t i = 0; i < LV_ADVANCED_KEYBOARD_SUGGESTION_COUNT; ++i)
                Check(lv_obj_has_state(lv_obj_get_child(suggestions, static_cast<int32_t>(i)), LV_STATE_DISABLED), "Malformed prediction remained clickable");
        }
        lookup.malformed = 0;
        lv_advanced_keyboard_set_text(object, "old word Ca"); Advance(80);
        Tap(suggestions, "Caf\xc3\xa9");
        Text(object, "old word Caf\xc3\xa9 ");
        Check(lv_textarea_get_cursor_pos(editor) == 14, "UTF-8 suggestion caret used byte indices");
        lv_advanced_keyboard_set_password(object, true); Advance(80);
        const auto callsWhilePassword = lookup.calls;
        lv_advanced_keyboard_set_text(object, "ca"); Advance(80);
        Check(lookup.calls == callsWhilePassword && !Button(suggestions, "caf\xc3\xa9"), "Password text was exposed to prediction lookup");
        lv_advanced_keyboard_set_password(object, false);
        lv_advanced_keyboard_set_suggestion_lookup(object, nullptr, nullptr);
        lv_advanced_keyboard_set_text(object, "\xc3\xa9\xf0\x9f\x99\x82" "ab");
        lv_advanced_keyboard_set_max_length(object, 3);
        Text(object, "\xc3\xa9\xf0\x9f\x99\x82" "a");
        lv_advanced_keyboard_set_password(object, true);
        lv_advanced_keyboard_set_text(object, "\xc3\xa9\xf0\x9f\x99\x82" "ab");
        lv_advanced_keyboard_set_password(object, false);
        Text(object, "\xc3\xa9\xf0\x9f\x99\x82" "a");
        lv_advanced_keyboard_set_text(object, "");
        Tap(keyboard, "a"); Tap(keyboard, "b"); Tap(keyboard, "c"); Tap(keyboard, "d");
        Text(object, "abc");

        lv_advanced_keyboard_set_max_length(object, 0);
        lv_advanced_keyboard_set_text(object, "Ready to type");
        lv_advanced_keyboard_set_placeholder(object, "");
        const auto original = ObjectSerializer::SerializeObject(object);
        Check(!original.at("advancedKeyboard").contains("text") && !original.at("advancedKeyboard").contains("placeholder"), "Keyboard serialized external field properties");
        for (const auto& bad : std::vector<json>{{{"targetOid", -2}}, {{"targetOid", 1.5}}, {{"keyRadius", -1}},
                                               {{"autoFocus", 1}}, {{"autocorrectThreshold", 2}}, {{"trieId", std::string("bad\0id", 6)}},
                                               {{"language", "unknown"}}, {{"language", 17}}}) {
            bool rejected = false;
            try { AdvancedKeyboard::Apply(object, bad); } catch (const std::exception&) { rejected = true; }
            Check(rejected, "Invalid Advanced Keyboard settings were accepted");
            Check(ObjectSerializer::SerializeObject(object).at("advancedKeyboard") == original.at("advancedKeyboard"), "Rejected settings changed keyboard state");
        }
        for (bool protobuf : {false, true}) {
            auto* restored = protobuf ? ObjectSerializer::FromProtobuf(ObjectSerializer::ToProtobuf(object)) : ObjectSerializer::DeserializeObject(original);
            Check(restored && AdvancedKeyboard::Type(restored), "Keyboard round trip lost native type");
            Check(ObjectSerializer::SerializeObject(restored).at("advancedKeyboard") == original.at("advancedKeyboard"), "JSON/Protobuf keyboard options changed");
            Check(!lv_advanced_keyboard_get_textarea(restored), "Keyboard round trip created an owned textarea");
            auto* external = ObjectSerializer::Create("textarea", previous);
            lv_obj_set_pos(external, 20, 10); lv_obj_set_size(external, 280, 60); lv_textarea_set_text(external, "Ready to type");
            Check(lv_advanced_keyboard_set_textarea(restored, external), "Could not attach restored keyboard");
            Tap(lv_advanced_keyboard_get_keyboard(restored), "x");
            Text(restored, "Ready to typex");
            lv_obj_delete(restored);
            Check(std::string(lv_textarea_get_text(external)) == "Ready to typex", "Deleting keyboard destroyed external field data");
            lv_obj_delete(external);
        }
        lv_advanced_keyboard_set_text(object, "Native text");
        Check(!ObjectSerializer::SerializeObject(object).at("advancedKeyboard").contains("text"), "Serialization copied external field text into keyboard");
        AdvancedKeyboard::Apply(object, original.at("advancedKeyboard"));
        {
            auto* composition = ObjectSerializer::Create("advancedKeyboard", previous);
            auto* panel = ObjectSerializer::Create("object", composition);
            auto* label = ObjectSerializer::Create("label", panel);
            lv_label_set_text(label, "Attached content");
            auto* nested = ObjectSerializer::Create("advancedKeyboard", panel);
            const auto record = ObjectSerializer::SerializeObject(composition);
            Check(record.at("contentChildren").size() == 1 && record.at("contentChildren")[0].at("contentChildren").size() == 2,
                  "Keyboard serialization exposed private generated children");
            Check(!record.at("contentChildren")[0].at("contentChildren")[1].contains("contentChildren"), "Nested keyboard serialized its private keys");
            auto* copy = ObjectSerializer::FromProtobuf(ObjectSerializer::ToProtobuf(composition));
            Check(lv_obj_get_child_count(copy) == 3, "Attached keyboard content did not round trip");
            auto* restoredPanel = lv_obj_get_child(copy, 2);
            Check(lv_obj_get_child_count(restoredPanel) == 2 && std::string(lv_label_get_text(lv_obj_get_child(restoredPanel, 0))) == "Attached content",
                  "Attached plain container/label hierarchy changed");
            Check(AdvancedKeyboard::Type(lv_obj_get_child(restoredPanel, 1)) && !lv_advanced_keyboard_get_textarea(lv_obj_get_child(restoredPanel, 1)), "Nested keyboard acquired an owned field");
            lv_obj_delete(copy); lv_obj_delete(composition);
        }
        Click(second); lv_textarea_set_cursor_pos(second, LV_TEXTAREA_CURSOR_LAST);
        Tap(keyboard, "x");
        Text(object, "Second fieldx");
        Check(std::string(lv_textarea_get_text(editor)) == "Native text", "Switching target erased the previous field");
        const auto callbacks = lv_obj_get_event_count(second);
        AdvancedKeyboard::BindTargets(previous, std::map<int, lv_obj_t*>{{7, object}, {8, editor}, {9, second}});
        Check(lv_obj_get_event_count(second) == callbacks && lv_advanced_keyboard_get_textarea(object) == second, "Repeated binding churned events or reset target");
        Click(inspector); Advance(20);
        Check(!lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) && lv_advanced_keyboard_get_textarea(object) == second, "Outside click dismissed the persistent keyboard");
        Check(AdvancedKeyboard::Normalize({{"hideOnDefocus", true}}).at("hideOnDefocus") == false, "Earlier hide-on-defocus records were not upgraded");
        lv_advanced_keyboard_set_hide_on_defocus(object, true);
        Click(second); Click(inspector); Advance(20);
        Check(lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN), "Explicit low-level legacy hide-on-defocus option stopped working");
        lv_advanced_keyboard_set_hide_on_defocus(object, false);
        Click(editor); lv_textarea_set_cursor_pos(editor, LV_TEXTAREA_CURSOR_LAST);
        Check(!lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN), "Refocus did not show keyboard");
        lv_obj_delete(second);
        auto* doomed = ObjectSerializer::Create("textarea", previous);
        Check(lv_advanced_keyboard_set_textarea(object, doomed), "Cannot attach temporary field");
        lv_obj_delete(doomed);
        Check(!lv_advanced_keyboard_get_textarea(object) && lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN), "Deleted target left a dangling keyboard attachment");
        auto* custom = CustomWidgets::Create("textBox", previous);
        lv_obj_set_pos(custom, 320, 80); lv_obj_set_size(custom, 280, 40);
        auto* customEntry = CustomWidgets::Part(custom, "Entry");
        Check(customEntry, "Custom Text box has no native entry");
        lv_textarea_set_text(customEntry, "Custom field");
        AdvancedKeyboard::Apply(object, {{"targetOid", 10}});
        AdvancedKeyboard::BindTargets(previous, std::map<int, lv_obj_t*>{{7, object}, {8, editor}, {10, custom}});
        Click(customEntry); lv_textarea_set_cursor_pos(customEntry, LV_TEXTAREA_CURSOR_LAST);
        Tap(keyboard, "q"); Text(object, "Custom fieldq");
        Check(lv_advanced_keyboard_bound_textarea_count(object) == 1, "Explicit Text box target also watched unrelated fields");
        lv_obj_delete(custom);
        Check(!lv_advanced_keyboard_get_textarea(object), "Deleted custom Text box left a dangling keyboard target");
        AdvancedKeyboard::Apply(object, {{"targetOid", -1}});
        AdvancedKeyboard::BindTargets(previous, std::map<int, lv_obj_t*>{{7, object}, {8, editor}});
        Click(editor);
        lv_advanced_keyboard_set_autocorrect(object, true);
        lv_advanced_keyboard_set_text(object, "teh");
        Tap(keyboard, "space");
        Text(object, "the ");
        lv_advanced_keyboard_set_text(object, "th"); Advance(100);
        Check(Button(lv_obj_get_child(object, 0), "the"), "Built-in ESPOS trie did not produce a real completion");
        Tap(lv_obj_get_child(object, 0), "the"); Text(object, "the ");
        lv_advanced_keyboard_set_text(object, "first\nsecond");
        Text(object, "first\nsecond");
        Check(!lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN), "Programmatic newline closed the keyboard");
        unsigned formCancelled = 0;
        lv_obj_add_event_cb(previous, CountEdits, LV_EVENT_CANCEL, &formCancelled);
        lv_obj_set_event_bubble(object, true);
        Click(lv_obj_get_child(lv_obj_get_child(object, 0), LV_ADVANCED_KEYBOARD_SUGGESTION_COUNT));
        Check(lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) && !lv_advanced_keyboard_get_textarea(object) && cancelled == 1 && fieldCancelled == 0 && formCancelled == 0,
              "X failed to close or cancelled the external field/form");
        lv_obj_set_event_bubble(object, false);
        lv_obj_remove_event_cb_with_user_data(previous, CountEdits, &formCancelled);
        Check(std::string(lv_textarea_get_text(editor)) == "first\nsecond", "X changed external text");
        AdvancedKeyboard::BindTargets(previous, std::map<int, lv_obj_t*>{{7, object}, {8, editor}});
        Check(lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) && lv_advanced_keyboard_is_dismissed(object), "Automatic rebind immediately reopened a dismissed keyboard");
        Click(editor); lv_obj_add_state(editor, LV_STATE_DISABLED);
        Tap(keyboard, "\xe2\x86\xb5");
        Check(lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) && !lv_advanced_keyboard_get_textarea(object) && std::string(lv_textarea_get_text(editor)) == "first\nsecond",
              "Done failed to close a disabled target or altered its text");
        lv_obj_remove_state(editor, LV_STATE_DISABLED);
        Click(editor); lv_advanced_keyboard_set_text(object, "teh");
        lv_obj_add_event_cb(editor, RefocusReady, LV_EVENT_READY, editor);
        uint32_t enter = LV_KEY_ENTER;
        lv_obj_send_event(editor, LV_EVENT_KEY, &enter);
        Check(std::string(lv_textarea_get_text(editor)) == "the" && lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) && !lv_advanced_keyboard_get_textarea(object),
              "Physical Enter failed to commit and stay closed after a READY focus callback");
        lv_obj_remove_event_cb_with_user_data(editor, RefocusReady, editor);
        {
            auto* temporary = ObjectSerializer::Create("advancedKeyboard", previous);
            auto* field = ObjectSerializer::Create("textarea", previous);
            lv_textarea_set_text(field, "kept");
            lv_advanced_keyboard_set_autocorrect(temporary, false);
            Check(lv_advanced_keyboard_set_textarea(temporary, field), "Cannot attach callback-deletion fixture");
            DeleteOnReady deletion{temporary};
            lv_obj_add_event_cb(field, DeleteKeyboardReady, LV_EVENT_READY, &deletion);
            lv_obj_send_event(field, LV_EVENT_KEY, &enter);
            Check(!deletion.keyboard && deletion.cleared && std::string(lv_textarea_get_text(field)) == "kept", "READY deletion accessed stale keyboard state or changed the field");
            lv_obj_delete(field);
        }
        Click(editor);
        lv_obj_delete(inspector);
        lv_obj_set_size(object, 600, 240); lv_obj_update_layout(object);
        VariableData variable;
        lv_textarea_set_max_length(editor, 64); lv_textarea_set_placeholder_text(editor, "Type a message"); lv_textarea_set_text(editor, "Hello");
        AdvancedKeyboard::Apply(object, {{"keyRadius", 6}, {"autocorrect", true}, {"language", "fr-FR"}});
        variable.name = "message"; variable.type = VariableType::String; variable.defaultValue = "Hello";
        variable.history = 4; variable.objectId = 8; variable.property = "/textarea/text";
        auto document = Project::ToJSON({object, editor}, previous, json::array({LVVariable::ToJSON(variable)}), 640, 480);
        document["obj"][0]["oid"] = 7;
        document["obj"][1]["oid"] = 8;
        document["runtimeTarget"] = "9.6";
        const auto binary = Protobuf::Encode(EntityKind::Project, "project", document);
        std::ofstream(directory / "fixture.json") << document.dump(2);
        std::ofstream stream(directory / "fixture.pb", std::ios::binary);
        stream.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size())); stream.close();
        CCodeGenerator generator;
        Check(generator.LoadProject(document), generator.GetLastError());
        json summary; Check(generator.Preflight(summary), generator.GetLastError());
        Check(generator.ExportProject(directory.u8string()), generator.GetLastError());
        for (const auto* target : {"9.5", "9.3", "9.2", "9.1", "8.3"}) {
            generator.SetTargetVersion(target);
            Check(!generator.ExportProject((directory / (std::string("unsupported-") + target)).u8string()) && generator.GetLastError().find("9.6") != std::string::npos,
                  "Older target did not reject Advanced Keyboard explicitly");
        }
        for (unsigned attempt = 0; attempt < 3; ++attempt) {
            lvgl_runtime_t* runtime = nullptr; char error[512]{};
            Check(lvgl_runtime_load_memory(binary.data(), binary.size(), nullptr, &runtime, error, sizeof error), error);
            auto* loaded = lvgl_runtime_object(runtime, 0, 7);
            Check(loaded && AdvancedKeyboard::Type(loaded), "Runtime lost Advanced Keyboard");
            auto* loadedEntry = lvgl_runtime_object(runtime, 0, 8);
            Check(loadedEntry && !lv_advanced_keyboard_get_textarea(loaded), "Runtime keyboard owns a textbox or auto-selects an unfocused target");
            Click(loadedEntry); lv_textarea_set_cursor_pos(loadedEntry, LV_TEXTAREA_CURSOR_LAST);
            Text(loaded, "Hello");
            lvgl_runtime_value_t value{}; value.type = LVGL_RUNTIME_STRING; value.string_value = "API text";
            Check(lvgl_runtime_set_variable(runtime, "message", &value), lvgl_runtime_error(runtime));
            Text(loaded, "API text");
            Tap(lv_advanced_keyboard_get_keyboard(loaded), "x");
            Check(lvgl_runtime_get_variable(runtime, "message", &value) && std::string(value.string_value) == "API textx", "Key input did not update bound variable");
            lv_textarea_set_text(lv_advanced_keyboard_get_textarea(loaded), "Edited \xc3\xa9");
            Check(lvgl_runtime_get_variable(runtime, "message", &value) && std::string(value.string_value) == "Edited \xc3\xa9", "External textarea edits did not update bound variable");
            value.string_value = "Fourth";
            Check(lvgl_runtime_set_variable(runtime, "message", &value), lvgl_runtime_error(runtime));
            value.string_value = "Fifth";
            Check(lvgl_runtime_set_variable(runtime, "message", &value), lvgl_runtime_error(runtime));
            Check(lvgl_runtime_history_count(runtime, "message") == 4, "Keyboard binding history did not remain bounded");
            auto invalid = document;
            invalid["obj"][0]["advancedKeyboard"]["targetOid"] = 999;
            const auto bad = Protobuf::Encode(EntityKind::Project, "project", invalid);
            lvgl_runtime_t* rejected = nullptr;
            Check(!lvgl_runtime_load_memory(bad.data(), bad.size(), nullptr, &rejected, error, sizeof error) && !rejected,
                  "Invalid external target was accepted");
            Check(lv_screen_active() == lvgl_runtime_screen(runtime, 0), "Rollback changed active external-target project");
            lv_refr_now(display);
            lvgl_runtime_destroy(runtime);
            Check(lv_screen_active() == previous, "Runtime keyboard lifetime did not restore active screen");
        }
        {
            auto legacy = document;
            legacy["obj"].erase(legacy["obj"].begin() + 1);
            legacy["obj"][0]["advancedKeyboard"] = {{"text", "Legacy text"}, {"placeholder", "Legacy placeholder"}, {"password", false}, {"maxLength", 32}};
            legacy["variables"][0]["var"]["oid"] = 7;
            legacy["variables"][0]["var"]["property"] = "/advancedKeyboard/text";
            legacy["variables"][0]["var"]["default"] = "Migrated value";
            const auto encoded = Protobuf::Encode(EntityKind::Project, "project", legacy);
            lvgl_runtime_t* runtime = nullptr; char error[512]{};
            Check(lvgl_runtime_load_memory(encoded.data(), encoded.size(), nullptr, &runtime, error, sizeof error), error);
            const auto migratedId = ObjectSerializer::SerializeObject(lvgl_runtime_object(runtime, 0, 7)).at("advancedKeyboard").at("targetOid").get<int>();
            auto* field = lvgl_runtime_object(runtime, 0, migratedId);
            Check(field && lv_obj_has_class(field, &lv_textarea_class), "Legacy owned editor was not migrated into an independent textarea");
            Check(std::string(lv_textarea_get_text(field)) == "Migrated value" && std::string(lv_textarea_get_placeholder_text(field)) == "Legacy placeholder" && lv_textarea_get_max_length(field) == 32,
                  "Legacy migration lost text, properties or variable association");
            Check(lv_obj_get_child_count(lvgl_runtime_object(runtime, 0, 7)) == 2, "Legacy migration retained an owned keyboard editor");
            lvgl_runtime_destroy(runtime);
        }
        Check(flushes > 0, "Keyboard never rendered into the native display");
        TestTargetReveal();
        TestSharedAncestorReveal();
        TestNestedViewportReveal();
        TestRevealCallbackDeletion(LV_EVENT_GET_SELF_SIZE);
        TestRevealCallbackDeletion(LV_EVENT_SCROLL_BEGIN);
        TestKeyboardLanguages();
        lv_obj_delete(object); lv_obj_delete(editor); lv_indev_delete(pointer); pointer = nullptr;
        lv_display_delete(display); lv_deinit();
        std::cout << "Advanced Keyboard clicks, shift/lock, symbols, UTF-8, prediction callback, serialization, runtime bindings and C export passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
