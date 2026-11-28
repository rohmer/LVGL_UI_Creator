#include <Serialization/AdvancedKeyboard.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Project.h>
#include <Serialization/ProjectKeyboard.h>
#include <Serialization/ProjectScreens.h>
#include <Serialization/Xml.h>
#include "../../CodeGeneration/CCodeGenerator.h"
#include "../../CodeGeneration/XmlCodeGenerator.h"
#include "../../CodeGeneration/MicroPythonCodeGenerator.h"
#include "../../Runtime/include/lvgl_runtime.h"
#include <lv_advanced_keyboard_prediction.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
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
        if (lv_obj_is_hidden(child) || !lv_obj_has_class(child, &lv_button_class)) continue;
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
json FieldStyles(lv_obj_t* object) { return LVObject::ToJSON(object).value("nativeStyles",json::array()); }
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
    state.cleared = lv_obj_is_hidden(state.keyboard) && !lv_advanced_keyboard_get_textarea(state.keyboard);
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
    Check(lv_obj_is_hidden(keyboard), "Reveal prevented keyboard dismissal");
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
    Check(lv_obj_is_hidden(keyboard) && !lv_advanced_keyboard_get_textarea(keyboard) &&
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
    if (!state.armed || !state.keyboard || lv_obj_is_hidden(state.keyboard) ||
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
    constexpr const char* ids[] = {"en-US", "es-ES", "fr-FR", "de-DE", "it-IT", "pt-PT", "zh-CN", "ar-SA", "hi-IN", "bn-BD", "id-ID", "ur-PK", "ru-RU", "el-GR"};
    Check(lv_advanced_keyboard_language_count() == 14, "Keyboard language catalog must contain all fourteen stable layouts");
    for (size_t i = 0; i < 14; ++i) {
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
        Check(lv_advanced_keyboard_get_textarea(keyboard) == field && !lv_obj_is_hidden(keyboard) &&
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

void TestScriptKeyboards(const std::filesystem::path& directory)
{
    auto* previous = lv_screen_active(); auto* screen = RevealScreen();
    auto* field = lv_textarea_create(screen); lv_obj_set_pos(field, 15, 15); lv_obj_set_size(field, 600, 65);
    auto* second = lv_textarea_create(screen); lv_obj_set_pos(second, 15, 100); lv_obj_set_size(second, 600, 65);
    lv_obj_set_style_text_font(field, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_align(field, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    const auto* authoredFont = lv_obj_get_style_text_font(field, LV_PART_MAIN);
    const auto authoredStyles = FieldStyles(field);
    auto* keyboard = RevealKeyboard(screen); auto* keys = lv_advanced_keyboard_get_keyboard(keyboard);
    lv_advanced_keyboard_set_autocorrect(keyboard, false);
    Check(lv_advanced_keyboard_set_textarea(keyboard, field), "Cannot attach script keyboard");
    struct Script { const char* id; const char* key; uint32_t glyph; bool rtl; };
    const Script scripts[] = {{"ar-SA",u8"س",0x0633,true},{"hi-IN",u8"क",0x0915,false},
        {"bn-BD",u8"ক",0x0995,false},{"id-ID","q",0x71,false},{"ur-PK",u8"پ",0x067e,true},
        {"ru-RU",u8"й",0x0439,false},{"zh-CN","n",0x4f60,false},{"el-GR",u8"α",0x03b1,false}};
    for (const auto& script : scripts) {
        Check(lv_advanced_keyboard_set_language(keyboard, script.id), std::string("Cannot select script ") + script.id);
        Check(lv_advanced_keyboard_get_right_to_left(keyboard) == script.rtl &&
            lv_obj_get_style_base_dir(field, LV_PART_MAIN) == (script.rtl ? LV_BASE_DIR_RTL : LV_BASE_DIR_LTR), "Layout direction did not reach the target");
        Check(lv_obj_get_style_text_font(field, LV_PART_MAIN) == authoredFont && lv_obj_get_style_text_align(field, LV_PART_MAIN) == LV_TEXT_ALIGN_CENTER,
            "Selecting a script replaced the target's authored font or alignment");
        Check(FieldStyles(field)==authoredStyles,"Editing direction leaked into authored field styles");
        auto* key = Button(keys, script.key); Check(key, "Script key is missing");
        const auto* font = lv_obj_get_style_text_font(key, LV_PART_MAIN); lv_font_glyph_dsc_t glyph{};
        Check(lv_font_get_glyph_dsc(font, &glyph, script.glyph, 0) && !glyph.is_placeholder && glyph.box_w && glyph.box_h, std::string("Missing compiled script glyph for ") + script.id);
        if (std::string(script.id) == "hi-IN" || std::string(script.id) == "bn-BD") {
            Check(lv_font_get_glyph_dsc(font, &glyph, 0x25cc, 0) && !glyph.is_placeholder && glyph.box_w, "Indic matra labels lack the dotted-circle glyph");
        }
        lv_textarea_set_text(field, "");
        if (std::string(script.id) != "zh-CN") { Tap(keys, script.key); Text(keyboard, script.key); Tap(keys, u8"⌫"); Text(keyboard, ""); }
        if (script.rtl) {
            lv_advanced_keyboard_set_right_to_left(keyboard, false);
            Check(!lv_advanced_keyboard_get_right_to_left(keyboard) && lv_obj_get_style_base_dir(field, LV_PART_MAIN) == LV_BASE_DIR_LTR,
                "RTL override could not select LTR");
            lv_advanced_keyboard_set_right_to_left(keyboard, true);
            Check(lv_advanced_keyboard_set_textarea(keyboard, second) && lv_obj_get_style_base_dir(second, LV_PART_MAIN) == LV_BASE_DIR_RTL,
                "RTL direction did not follow a switched target");
            Check(lv_advanced_keyboard_set_textarea(keyboard, field), "Cannot restore script target");
        }
        const bool savedRTL = std::string(script.id) == "ar-SA" ? false : script.rtl;
        lv_advanced_keyboard_set_right_to_left(keyboard,savedRTL);
        auto record = ObjectSerializer::SerializeObject(keyboard); record["oid"] = 7; record["par"] = -1; record["advancedKeyboard"]["targetOid"] = 8;
        Check(record["advancedKeyboard"]["language"] == script.id && record["advancedKeyboard"]["rightToLeft"] == savedRTL,
            "Script or direction did not serialize");
        auto entry = ObjectSerializer::SerializeObject(field); entry["oid"] = 8; entry["par"] = -1;
        json document={{"canvas",{{"width",640},{"height",480}}},{"variables",json::array()},{"obj",json::array({record,entry})},{"runtimeTarget","9.6"}};
        document = json::parse(document.dump()); const auto binary = Protobuf::Encode(EntityKind::Project,"project",document);
        Check(Protobuf::Decode(binary).data["obj"][0]["advancedKeyboard"]["rightToLeft"] == savedRTL, "Protobuf lost RTL setting");
        const auto output = directory / "languages" / script.id; CCodeGenerator generator;
        Check(generator.LoadProject(document) && generator.ExportProject(output.u8string()), generator.GetLastError());
        std::ifstream generated(output / "ui.c"); const std::string code((std::istreambuf_iterator<char>(generated)),{});
        Check(code.find("lv_advanced_keyboard_set_right_to_left") != std::string::npos && code.find(script.id) != std::string::npos, "Generated C omitted script or direction");
        lvgl_runtime_t* runtime = nullptr; char error[512]{};
        Check(lvgl_runtime_load_memory(binary.data(),binary.size(),nullptr,&runtime,error,sizeof error),error);
        auto* loaded=lvgl_runtime_object(runtime,0,7); auto* target=lvgl_runtime_object(runtime,0,8);
        Check(loaded && target && std::string(lv_advanced_keyboard_get_language(loaded))==script.id && lv_advanced_keyboard_get_right_to_left(loaded)==savedRTL,
            "Runtime lost the script layout or direction");
        Check(lv_advanced_keyboard_set_textarea(loaded,target) && lv_obj_get_style_base_dir(target,LV_PART_MAIN)==(savedRTL?LV_BASE_DIR_RTL:LV_BASE_DIR_LTR),
            "Runtime did not apply direction to the bound target");
        lvgl_runtime_destroy(runtime); Check(lv_screen_active()==screen,"Script runtime did not restore the native screen");
    }
    Check(lv_advanced_keyboard_set_language(keyboard,"ar-SA"),"Cannot select Arabic");
    lv_textarea_set_text(field,""); for(const auto* letter:{u8"س",u8"ل",u8"ا",u8"م"})Tap(keys,letter);
    Text(keyboard,u8"سلام"); Check(lv_textarea_get_cursor_pos(field)==4,"Arabic lam/alef collapsed logical cursor positions");
    Tap(keys,u8"⌫"); Text(keyboard,u8"سلا"); Check(lv_textarea_get_cursor_pos(field)==3,"Arabic backspace removed the wrong logical character");
    Check(lv_advanced_keyboard_set_language(keyboard,"ur-PK"),"Cannot select Urdu");
    lv_textarea_set_text(field,""); for(const auto* letter:{u8"پ",u8"ی",u8"ا",u8"ر"})Tap(keys,letter);
    Text(keyboard,u8"پیار"); Tap(keys,u8"⌫"); Text(keyboard,u8"پیا");
    Check(lv_advanced_keyboard_set_language(keyboard,"hi-IN"),"Cannot select Hindi");
    lv_textarea_set_text(field,"");Tap(keys,u8"क");Tap(keys,u8"◌ि");Text(keyboard,u8"कि");Tap(keys,u8"⌫");Text(keyboard,u8"क");
    Check(lv_advanced_keyboard_set_language(keyboard,"bn-BD"),"Cannot select Bengali");
    lv_textarea_set_text(field,"");Tap(keys,u8"ক");Tap(keys,u8"◌ি");Text(keyboard,u8"কি");Tap(keys,u8"⌫");Text(keyboard,u8"ক");
    Check(lv_advanced_keyboard_set_language(keyboard,"ru-RU"),"Cannot select Russian");
    lv_textarea_set_text(field,"");Tap(keys,u8"й");Tap(keys,u8"⇧");Advance(320);Tap(keys,u8"Й");Text(keyboard,u8"йЙ");
    Check(lv_advanced_keyboard_set_language(keyboard,"el-GR"),"Cannot select Greek");
    lv_textarea_set_text(field,"");Tap(keys,u8"α");Tap(keys,u8"⇧");Advance(320);Tap(keys,u8"Α");
    Tap(keys,u8"άέί");Tap(keys,u8"ά");Text(keyboard,u8"αΑά");
    Check(lv_advanced_keyboard_set_language(keyboard,"zh-CN"),"Cannot select Pinyin");
    auto* bar=lv_obj_get_child(keyboard,0);lv_textarea_set_text(field,"");Tap(keys,"n");Tap(keys,"i");Text(keyboard,"");
    Check(Button(bar,u8"你"),"Pinyin ni did not offer a real Chinese candidate");Tap(bar,u8"你");Text(keyboard,u8"你");
    lv_textarea_add_text(field,"hao");Text(keyboard,u8"你");Check(Button(bar,u8"好"),"Physical Pinyin input did not compose");Tap(keys,"space");Text(keyboard,u8"你好");
    Tap(keys,"s");Tap(keys,"h");Tap(keys,"i");Check(Button(bar,u8"是"),"Pinyin shi has no first candidate");
    Tap(bar,">");Check(!Button(bar,u8"是"),"Pinyin next page repeated the first page");Tap(bar,"<");Check(Button(bar,u8"是"),"Pinyin previous page did not restore candidates");Tap(bar,u8"是");Text(keyboard,u8"你好是");
    lv_textarea_set_text(field,"");lv_textarea_add_text(field,"shi");Tap(bar,">");Check(Button(bar,u8"食"),"Pinyin second page has no expected character");Tap(keys,"space");Text(keyboard,u8"食");
    lv_textarea_set_text(field,"");lv_textarea_add_text(field,"ni");Tap(keys,u8"⌫");Text(keyboard,"");Tap(keys,u8"⌫");Check(!Button(bar,u8"你"),"Backspace retained a stale Pinyin candidate");
    lv_textarea_add_text(field,"ni");Check(lv_advanced_keyboard_set_textarea(keyboard,second),"Cannot switch Pinyin target");lv_textarea_set_text(second,"");
    Tap(keys,"space");Check(std::string(lv_textarea_get_text(field)).empty()&&std::string(lv_textarea_get_text(second))==" ","Pinyin preedit leaked between text boxes");
    Check(lv_advanced_keyboard_set_textarea(keyboard,field),"Cannot restore Pinyin target");lv_textarea_add_text(field,"ni");
    Check(lv_advanced_keyboard_set_language(keyboard,"ar-SA")&&lv_advanced_keyboard_set_language(keyboard,"zh-CN"),"Cannot switch away from Pinyin");
    Check(!Button(bar,u8"你"),"Language switch retained private Pinyin state");lv_textarea_add_text(field,"ni");Tap(bar,"X");
    Check(!lv_advanced_keyboard_get_textarea(keyboard)&&std::string(lv_textarea_get_text(field)).empty(),"X committed private Pinyin composition");
    Check(lv_advanced_keyboard_set_textarea(keyboard,field),"Cannot reopen Pinyin keyboard");lv_textarea_add_text(field,"ni");
    lv_advanced_keyboard_set_password(keyboard,true);Check(!Button(bar,u8"你"),"Password mode retained Chinese candidates");
    lv_textarea_add_text(field,"hao");Text(keyboard,"hao");lv_advanced_keyboard_set_password(keyboard,false);
    lv_textarea_set_text(field,"");lv_textarea_add_text(field,"ni");uint32_t enter=LV_KEY_ENTER;lv_obj_send_event(field,LV_EVENT_KEY,&enter);
    Check(!lv_advanced_keyboard_get_textarea(keyboard)&&std::string(lv_textarea_get_text(field))==u8"你","Physical Enter did not commit Pinyin and dismiss");
    Check(FieldStyles(field)==authoredStyles,"Dismissing a script keyboard did not restore authored direction");
    auto* peer=RevealKeyboard(screen);Check(lv_advanced_keyboard_set_language(peer,"ar-SA")&&lv_advanced_keyboard_set_textarea(peer,field),"Cannot attach shared RTL field");
    Check(lv_advanced_keyboard_set_textarea(keyboard,field)&&lv_obj_get_style_base_dir(field,LV_PART_MAIN)==LV_BASE_DIR_LTR,"Newest keyboard did not control shared-field direction");
    lv_advanced_keyboard_close(keyboard);Check(lv_obj_get_style_base_dir(field,LV_PART_MAIN)==LV_BASE_DIR_RTL,"Closing one keyboard disrupted another keyboard's direction");
    Check(FieldStyles(field)==authoredStyles,"Shared keyboard directions leaked into serialization");
    lv_obj_set_style_base_dir(field,LV_BASE_DIR_LTR,LV_PART_MAIN);const auto editedStyles=FieldStyles(field);
    lv_advanced_keyboard_close(peer);Check(lv_obj_get_style_base_dir(field,LV_PART_MAIN)==LV_BASE_DIR_LTR&&FieldStyles(field)==editedStyles,"Dismissing keyboard discarded an authored direction edit");
    lv_obj_delete(peer);
    lv_screen_load(previous);lv_obj_delete(screen);
    std::cout<<"Fourteen layouts, real script glyphs, canonical RTL text, Indic matras, Pinyin candidates/paging and script JSON/Protobuf/C/runtime persistence passed\n";
}

std::vector<uint8_t> ScriptPredictionFixture()
{
    struct Node { uint8_t label=0; uint32_t terminal=0; std::map<uint8_t,std::unique_ptr<Node>> children; };
    Node all, unigrams;
    auto insert=[](Node& root,const std::string& text,uint32_t count){auto* node=&root;for(uint8_t value:text){auto& child=node->children[value];if(!child){child=std::make_unique<Node>();child->label=value;}node=child.get();}node->terminal=count;};
    for(const auto& word:std::vector<std::pair<std::string,uint32_t>>{{u8"سلام",100},{u8"सुंदर",100},{u8"সুন্দর",100},{u8"پاکستان",100},{u8"привет",100},{"saya",100},{u8"γεια",100},
        {u8"κόσμος",1000},{u8"μας",1000},{u8"φίλος",1000},{u8"κόσμος μας",1000},{u8"μας φίλος",1000},{u8"κόσμος μας φίλος",1000},
        {u8"μας κόσμος",1000},{u8"κόσμος μας κόσμος",1000},
        {u8"straße",1000},{u8"grüße",1000},{u8"schön",1000},{u8"straße grüße",1000},{u8"grüße schön",1000},{u8"straße grüße schön",1000},
        {u8"grüße straße",1000},{u8"straße grüße straße",1000},
        {u8"\u0915\u093c\u0932\u092e",100},{u8"\u09a1\u09bc\u09be\u0995",100},
        {u8"你",1000},{u8"好",1000},{u8"吗",1000},{u8"你 好",1000},{u8"好 吗",1000},{u8"你 好 吗",1000}}){
        insert(all,word.first,word.second);if(word.first.find(' ')==std::string::npos)insert(unigrams,word.first,word.second);
    }
    std::vector<Node*> nodes{&all,&unigrams};for(size_t i=0;i<nodes.size();++i)for(const auto& child:nodes[i]->children)nodes.push_back(child.second.get());
    std::vector<uint8_t> bytes{'N','G','T','R'};
    auto u32=[&](uint32_t value){for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<uint8_t>(value>>(8*i)));};
    auto var=[&](uint32_t value){while(value>=128){bytes.push_back(static_cast<uint8_t>(value|128));value>>=7;}bytes.push_back(static_cast<uint8_t>(value));};
    u32(3);u32(static_cast<uint32_t>(nodes.size()));u32(0);
    for(const auto* node:nodes){bytes.push_back(node->label);var(static_cast<uint32_t>((node->children.size()<<1)|(node->terminal?1:0)));if(node->terminal)var(node->terminal);}
    return bytes;
}
void TestScriptPredictions(const std::filesystem::path& directory)
{
    auto* previous=lv_screen_active();auto* screen=RevealScreen();auto* field=lv_textarea_create(screen);lv_obj_set_pos(field,15,15);lv_obj_set_size(field,600,65);
    auto* keyboard=RevealKeyboard(screen);auto* bar=lv_obj_get_child(keyboard,0);lv_advanced_keyboard_set_autocorrect(keyboard,false);
    const auto bytes=ScriptPredictionFixture();auto* predictor=lv_advanced_keyboard_predictor_create(bytes.data(),bytes.size());Check(predictor,"Cannot create script prediction model");
    lv_advanced_keyboard_set_predictor(keyboard,predictor);Check(lv_advanced_keyboard_set_textarea(keyboard,field),"Cannot bind script prediction field");
    struct Case {const char* language;const char* prefix;const char* suggestion;};
    for(const auto& item:std::vector<Case>{{"ar-SA",u8"سل",u8"سلام"},{"hi-IN",u8"सु",u8"सुंदर"},{"bn-BD",u8"সু",u8"সুন্দর"},{"ur-PK",u8"پا",u8"پاکستان"},{"ru-RU",u8"пр",u8"привет"},{"ru-RU",u8"ПР",u8"ПРИВЕТ"},{"id-ID","sa","saya"},{"el-GR",u8"γε",u8"γεια"},
        {"el-GR",u8"κόσμο",u8"κόσμος"},{"el-GR",u8"ΚΌΣΜΟ",u8"ΚΌΣΜΟΣ"},{"de-DE",u8"straß",u8"straße"},{"de-DE",u8"STRAẞ",u8"STRAẞE"},
        {"hi-IN",u8"\u0958",u8"\u0915\u093c\u0932\u092e"},{"bn-BD",u8"\u09dc",u8"\u09a1\u09bc\u09be\u0995"}}){
        Check(lv_advanced_keyboard_set_language(keyboard,item.language),"Cannot choose prediction script");lv_textarea_set_text(field,item.prefix);lv_textarea_set_cursor_pos(field,LV_TEXTAREA_CURSOR_LAST);Advance(80);
        Check(Button(bar,item.suggestion),std::string("Trie prefix suggestions are missing for ")+item.language);Tap(bar,item.suggestion);Text(keyboard,std::string(item.suggestion)+" ");
    }
    for(const auto& item:std::vector<Case>{{"el-GR",u8"ΚΌΣΜΟΣ ΜΑΣ ",u8"φίλος"},{"de-DE",u8"STRAẞE GRÜẞE ",u8"schön"}}) {
        Check(lv_advanced_keyboard_set_language(keyboard,item.language),"Cannot choose listed-spelling context language");
        lv_textarea_set_text(field,item.prefix);lv_textarea_set_cursor_pos(field,LV_TEXTAREA_CURSOR_LAST);Advance(80);
        Check(Button(bar,item.suggestion),std::string("Listed-spelling previous/older context lost sigma or sharp S for ")+item.language);
        Tap(bar,item.suggestion);Text(keyboard,std::string(item.prefix)+item.suggestion+" ");
    }
    lv_advanced_keyboard_set_autocorrect(keyboard,true);lv_advanced_keyboard_set_autocorrect_threshold(keyboard,0);
    for(const auto& item:std::vector<Case>{{"el-GR",u8"ΚΌΣΜΟΣ ΜΑΣ κόσμος",u8"ΚΌΣΜΟΣ ΜΑΣ κόσμος"},{"el-GR",u8"ΚΌΣΜΟΣ ΜΑΣ κόσμο",u8"ΚΌΣΜΟΣ ΜΑΣ κόσμος"},
        {"de-DE",u8"STRAẞE GRÜẞE straße",u8"STRAẞE GRÜẞE straße"},{"de-DE",u8"STRAẞE GRÜẞE straß",u8"STRAẞE GRÜẞE straße"}}) {
        Check(lv_advanced_keyboard_set_language(keyboard,item.language),"Cannot choose listed-spelling autocorrect language");
        lv_textarea_set_text(field,item.prefix);lv_textarea_set_cursor_pos(field,LV_TEXTAREA_CURSOR_LAST);
        uint32_t enter=LV_KEY_ENTER;lv_obj_send_event(field,LV_EVENT_KEY,&enter);
        Check(std::string(lv_textarea_get_text(field))==item.suggestion,std::string("Boundary autocorrect folded listed spelling incorrectly for ")+item.language+
            ": typed '"+item.prefix+"', expected '"+item.suggestion+"', got '"+lv_textarea_get_text(field)+"'");
        Check(lv_advanced_keyboard_set_textarea(keyboard,field),"Cannot reopen listed-spelling keyboard");
    }
    lv_advanced_keyboard_set_autocorrect(keyboard,false);lv_advanced_keyboard_set_autocorrect_threshold(keyboard,0.3);
    for(const auto& item:std::vector<Case>{{"hi-IN",u8"\u0958\u0932\u092e",u8"\u0958\u0932\u092e"},{"bn-BD",u8"\u09dc\u09be\u0995",u8"\u09dc\u09be\u0995"}}){
        Check(lv_advanced_keyboard_set_language(keyboard,item.language),"Cannot choose Indic autocorrect script");lv_advanced_keyboard_set_autocorrect(keyboard,true);
        lv_textarea_set_text(field,item.prefix);lv_textarea_set_cursor_pos(field,LV_TEXTAREA_CURSOR_LAST);Tap(lv_advanced_keyboard_get_keyboard(keyboard),"space");Text(keyboard,std::string(item.suggestion)+" ");
        lv_advanced_keyboard_set_autocorrect(keyboard,false);
    }
    Check(lv_advanced_keyboard_set_language(keyboard,"zh-CN"),"Cannot choose Han prediction mode");lv_textarea_set_text(field,u8"你好");lv_textarea_set_cursor_pos(field,LV_TEXTAREA_CURSOR_LAST);Advance(80);
    Check(Button(bar,u8"吗"),"Mandarin trie did not use the last two Han character tokens");Tap(bar,u8"吗");Text(keyboard,u8"你好吗");
    lv_advanced_keyboard_set_autocorrect(keyboard,true);lv_textarea_set_text(field,"");lv_textarea_add_text(field,"ni");Tap(bar,u8"你");lv_textarea_add_text(field,"hao");Tap(lv_advanced_keyboard_get_keyboard(keyboard),"space");
    unsigned ready=0;lv_obj_add_event_cb(field,[](lv_event_t* event){++*static_cast<unsigned*>(lv_event_get_user_data(event));},LV_EVENT_READY,&ready);
    uint32_t enter=LV_KEY_ENTER;lv_obj_send_event(field,LV_EVENT_KEY,&enter);
    Check(ready==1&&!lv_advanced_keyboard_get_textarea(keyboard)&&std::string(lv_textarea_get_text(field))==u8"你好","Mandarin Enter/autocorrect rewrote committed Han text or lost READY");
    Check(lv_advanced_keyboard_set_textarea(keyboard,field),"Cannot reopen contextual Mandarin keyboard");Advance(80);Check(Button(bar,u8"吗"),"Mandarin next-character context was not retained after Enter");Tap(bar,u8"吗");Text(keyboard,u8"你好吗");
    auto unconfigured=ObjectSerializer::SerializeObject(keyboard);unconfigured["oid"]=7;unconfigured["par"]=-1;unconfigured["advancedKeyboard"].erase("trieId");
    std::string trieId;const auto catalog=ProjectTries::Import(json::array(),"script-model",bytes.data(),bytes.size(),&trieId);
    auto configured=unconfigured;configured["oid"]=8;configured["advancedKeyboard"]["language"]="ar-SA";configured["advancedKeyboard"]["trieId"]=trieId;
    json document={{"canvas",{{"width",640},{"height",480}}},{"obj",json::array({unconfigured,configured})},{"tries",catalog},{"runtimeTarget","9.6"}};
    const auto exportDirectory=directory/"no-unused-english";CCodeGenerator generator;
    Check(generator.LoadProject(document)&&generator.ExportProject(exportDirectory.u8string()),generator.GetLastError());
    std::ifstream cmakeFile(exportDirectory/"CMakeLists.txt");const std::string cmake((std::istreambuf_iterator<char>(cmakeFile)),{});
    Check(cmake.find("LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION=1")!=std::string::npos&&cmake.find("LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE=0")!=std::string::npos,
        "Exporting a non-English default with a custom model retained the unused English trie");
    lv_advanced_keyboard_set_predictor(keyboard,nullptr);lv_advanced_keyboard_predictor_destroy(predictor);lv_screen_load(previous);lv_obj_delete(screen);
    std::cout<<"Real Arabic, Indic, Urdu, Cyrillic, Indonesian trie prefixes and unspaced Mandarin trigram prediction passed\n";
}
}

namespace {
void TestLongPressVariants()
{
    auto* previous = lv_screen_active();
    auto* screen = RevealScreen();
    auto* display = lv_obj_get_display(screen);
    auto* top = lv_display_get_layer_top(display);
    const auto topChildren = lv_obj_get_child_count(top);
    auto* field = lv_textarea_create(screen);
    lv_obj_set_pos(field, 12, 12); lv_obj_set_size(field, 600, 70);
    lv_textarea_set_text(field, "");
    auto* object = lv_advanced_keyboard_create(screen);
    Check(object, "Cannot create long-press fixture keyboard");
    lv_obj_set_pos(object, 0, 240); lv_obj_set_size(object, 640, 240);
    Check(lv_advanced_keyboard_set_textarea(object, field), "Cannot attach long-press fixture field");
    lv_advanced_keyboard_set_autocorrect(object, false);
    auto* keyboard = lv_advanced_keyboard_get_keyboard(object);
    lv_obj_update_layout(screen);
    const auto popup = [&]() -> lv_obj_t* {
        if (lv_obj_get_child_count(top) <= topChildren) return nullptr;
        auto* overlay = lv_obj_get_child(top, -1);
        Check(lv_obj_get_child_count(overlay) == 1, "Variant overlay must contain one row");
        auto* row = lv_obj_get_child(overlay, 0);
        lv_obj_update_layout(row);
        lv_area_t bounds{}; lv_obj_get_coords(row, &bounds);
        Check(bounds.x1 >= 0 && bounds.y1 >= 0 &&
              bounds.x2 < lv_display_get_horizontal_resolution(display) &&
              bounds.y2 < lv_display_get_vertical_resolution(display), "Variant row escaped display bounds");
        return row;
    };
    const auto press = [&](lv_obj_t* key) {
        Check(key, "Missing long-press source key");
        lv_obj_update_layout(key);
        lv_area_t bounds{}; lv_obj_get_coords(key, &bounds);
        pointerPoint = {static_cast<int32_t>((bounds.x1 + bounds.x2) / 2),
                        static_cast<int32_t>((bounds.y1 + bounds.y2) / 2)};
        pointerState = LV_INDEV_STATE_RELEASED; lv_indev_read(pointer);
        pointerState = LV_INDEV_STATE_PRESSED; lv_indev_read(pointer);
    };
    const auto release = [&]() {
        pointerState = LV_INDEV_STATE_RELEASED;
        lv_indev_read(pointer);
    };
    const auto hold = [&](const char* caption) {
        press(Button(keyboard, caption));
        Advance(1000);
        Check(popup(), std::string("No long-press row for ") + caption);
        release();
        Check(popup(), "Releasing held source dismissed the choices");
    };
    const auto holdContinuously = [&](const char* caption) {
        press(Button(keyboard, caption));
        Advance(1000);
        Check(popup(), std::string("No continuous-hold row for ") + caption);
    };
    const auto moveTo = [&](lv_obj_t* target) {
        Check(target, "Missing drag selection target");
        lv_obj_update_layout(target);
        lv_area_t bounds{}; lv_obj_get_coords(target, &bounds);
        pointerPoint = {static_cast<int32_t>((bounds.x1 + bounds.x2) / 2),
                        static_cast<int32_t>((bounds.y1 + bounds.y2) / 2)};
        Check(pointerState == LV_INDEV_STATE_PRESSED, "Drag test accidentally released its original press");
        lv_indev_read(pointer); Advance(20);
    };
    const auto moveOutside = [&]() {
        Check(pointerState == LV_INDEV_STATE_PRESSED, "Outside drag lost its original press");
        pointerPoint = {lv_display_get_horizontal_resolution(display) - 10, 15};
        lv_indev_read(pointer); Advance(20);
    };
    const auto dragChoice = [&](const char* caption) {
        auto* choice = Button(popup(), caption);
        Check(choice, std::string("Missing drag variant ") + caption);
        moveTo(choice);
        Check(lv_obj_has_state(choice, LV_STATE_PRESSED), "Variant under the held pointer was not highlighted");
        return choice;
    };
    const auto finishDrag = [&](const char* expected) {
        release(); Advance(60);
        Check(!popup(), "Release over a dragged variant did not dismiss its popup");
        Text(object, expected);
        lv_indev_read(pointer); Advance(60);
        Text(object, expected);
    };
    const auto choose = [&](const char* caption) {
        auto* row = popup();
        Check(row, "Missing variant row before selection");
        auto* choice = Button(row, caption);
        Check(choice, std::string("Missing long-press choice ") + caption);
        Click(choice);
        Check(!popup(), "Variant row remained after acceptance");
    };
    const auto blank = [&]() { lv_textarea_set_text(field, ""); };

    // These use the actual LVGL pointer device, including its pressed-target
    // capture and release handling when a modal row appears above the source.
    press(Button(keyboard, "a"));
    Advance(999);
    Check(!popup(), "Variant row appeared before a full second");
    Check(std::strcmp(lv_textarea_get_text(field), "") == 0, "Holding inserted a source character before release");
    Advance(1);
    Check(popup(), "Variant row did not appear at one second");
    release();
    Text(object, "");
    Check(popup(), "Source release removed long-press row");
    choose(u8"à");
    Text(object, u8"à");
    Tap(keyboard, "b");
    Text(object, u8"àb");

    // Keep the original press down while moving onto and between choices.
    auto* scrollExtent = lv_obj_create(screen);
    lv_obj_remove_style_all(scrollExtent); lv_obj_set_pos(scrollExtent, 0, 900); lv_obj_set_size(scrollExtent, 1, 1);
    lv_obj_set_clickable(scrollExtent, false); lv_obj_update_layout(screen);
    const auto ancestorScrollBefore = lv_obj_get_scroll_y(screen);
    blank(); holdContinuously("a"); Text(object, "");
    auto* firstHovered = dragChoice(u8"à");
    auto* secondHovered = dragChoice(u8"á");
    Check(!lv_obj_has_state(firstHovered, LV_STATE_PRESSED), "Moving between variants left the old choice highlighted");
    moveOutside();
    Check(!lv_obj_has_state(secondHovered, LV_STATE_PRESSED), "Leaving the row retained a highlighted choice");
    dragChoice(u8"ä");
    finishDrag(u8"ä");
    Check(lv_obj_get_scroll_y(screen) == ancestorScrollBefore, "Dragging a variant scrolled the keyboard's ancestor");
    lv_obj_delete(scrollExtent);
    Tap(keyboard, "b"); Text(object, u8"äb");
    blank(); holdContinuously("a");
    auto* abandoned = dragChoice(u8"á");
    moveOutside();
    Check(!lv_obj_has_state(abandoned, LV_STATE_PRESSED), "Dragging outside retained a candidate");
    release(); Advance(60);
    Text(object, "");
    if (auto* unselectedRow = popup()) {
        for (uint32_t index = 0; index < lv_obj_get_child_count(unselectedRow); ++index)
            Check(!lv_obj_has_state(lv_obj_get_child(unselectedRow, index), LV_STATE_PRESSED), "Releasing outside retained a drag highlight");
        pointerState = LV_INDEV_STATE_PRESSED; lv_indev_read(pointer); release();
    }
    Check(!popup(), "Outside tap did not dismiss the unselected row");
    Tap(keyboard, "b"); Text(object, "b");

    blank();
    press(Button(keyboard, "a")); Advance(999); release();
    Check(!popup(), "Short press created a variant row");
    Text(object, "a");
    blank();
    press(Button(keyboard, "a")); Advance(400);
    pointerPoint = {630, 30}; lv_indev_read(pointer);
    Advance(800); release();
    Check(!popup(), "Dragging away from a key still opened its variants");
    Text(object, "");
    press(Button(keyboard, "b")); Advance(1200); release();
    Check(!popup(), "Key with no variants opened an empty row");
    Text(object, "b");

    blank();
    Tap(keyboard, u8"⇧"); Advance(320);
    hold("A");
    for (const char* caption : {u8"À", u8"Á", u8"Â", u8"Ã", u8"Ä", u8"Å"})
        Check(Button(popup(), caption), std::string("Uppercase A is missing ") + caption);
    choose(u8"À");
    Text(object, u8"À");
    Check(Button(keyboard, "a") && !Button(keyboard, "A"), "Variant acceptance did not consume one-shot Shift");
    Tap(keyboard, u8"⇧"); Tap(keyboard, u8"⇧");
    Check(Button(keyboard, u8"⇪"), "Double-tap did not preserve Shift Lock");
    hold("A"); choose(u8"Á");
    Text(object, u8"ÀÁ");
    Check(Button(keyboard, "A") && Button(keyboard, u8"⇪"), "Variant acceptance consumed Shift Lock");
    Tap(keyboard, u8"⇪"); Advance(320);

    blank(); Tap(keyboard, u8"⇧"); Advance(320);
    holdContinuously("A"); dragChoice(u8"Ä"); finishDrag(u8"Ä");
    Check(Button(keyboard, "a") && !Button(keyboard, "A"), "Drag acceptance did not consume one-shot Shift");
    Tap(keyboard, u8"⇧"); Tap(keyboard, u8"⇧");
    holdContinuously("A"); dragChoice(u8"Å"); finishDrag(u8"ÄÅ");
    Check(Button(keyboard, "A") && Button(keyboard, u8"⇪"), "Drag acceptance consumed Shift Lock");
    Tap(keyboard, u8"⇪"); Advance(320);

    // Popups open inward from either edge rather than continuing outward.
    blank();
    auto* leftSource = Button(keyboard, "a");
    lv_area_t leftBounds{}; lv_obj_get_coords(leftSource, &leftBounds);
    hold("a");
    lv_area_t leftPopup{}; lv_obj_get_coords(popup(), &leftPopup);
    Check(leftPopup.x1 > leftBounds.x2, "A left-half key did not open choices to its right");
    choose(u8"à");
    blank();
    auto* rightSource = Button(keyboard, "o");
    lv_area_t rightBounds{}; lv_obj_get_coords(rightSource, &rightBounds);
    hold("o");
    lv_area_t rightPopup{}; lv_obj_get_coords(popup(), &rightPopup);
    Check(rightPopup.x2 < rightBounds.x1, "A right-half key did not open choices to its left");
    choose(u8"ò");
    blank(); holdContinuously("o"); dragChoice(u8"ö"); finishDrag(u8"ö");

    struct LanguageCase { const char* id; const char* key; const char* variant; };
    constexpr LanguageCase languages[] = {
        {"en-US", "a", u8"à"}, {"es-ES", "a", u8"á"}, {"fr-FR", "a", u8"à"},
        {"de-DE", "a", u8"ä"}, {"it-IT", "a", u8"à"}, {"pt-PT", "a", u8"ã"},
        {"zh-CN", "a", u8"ā"}, {"ar-SA", u8"ا", u8"آ"},
        {"hi-IN", u8"क", u8"क़"}, {"bn-BD", u8"ড", u8"ড়"},
        {"id-ID", "a", u8"á"}, {"ur-PK", u8"ا", u8"آ"},
        {"ru-RU", u8"е", u8"ё"}, {"el-GR", u8"α", u8"ά"},
    };
    for (const auto& language : languages) {
        Check(lv_advanced_keyboard_set_language(object, language.id), std::string("Cannot select ") + language.id);
        blank(); hold(language.key); choose(language.variant);
        Text(object, language.variant);
        blank(); holdContinuously(language.key); dragChoice(language.variant); finishDrag(language.variant);
    }

    Check(lv_advanced_keyboard_set_language(object, "en-US"), "Cannot restore English");
    blank(); hold("a");
    pointerPoint = {630, 15};
    pointerState = LV_INDEV_STATE_PRESSED; lv_indev_read(pointer);
    release();
    Check(!popup(), "Outside tap did not dismiss the variant row");
    Text(object, "");
    hold("a");
    Check(lv_advanced_keyboard_set_language(object, "fr-FR"), "Cannot change language during variants");
    Check(!popup(), "Language switch left stale variant choices");
    blank(); holdContinuously("a"); dragChoice(u8"à");
    Check(lv_advanced_keyboard_set_language(object, "en-US"), "Cannot change language during drag");
    release(); Advance(60);
    Check(!popup(), "Language switch left a drag selection active"); Text(object, "");
    blank(); hold("a");
    lv_obj_set_width(object, 600); lv_obj_update_layout(object);
    Check(!popup(), "Resizing left a stale variant row");
    lv_obj_set_width(object, 640); lv_obj_update_layout(object);
    hold("a");
    lv_obj_set_hidden(object, true); Advance(60);
    Check(!popup(), "Hiding the keyboard left an active variant row");
    lv_obj_set_hidden(object, false);
    // Visibility can change between two input samples, before the periodic
    // cleanup timer observes it. A release must recheck the active owner.
    blank(); holdContinuously("a"); dragChoice(u8"à");
    lv_obj_set_hidden(object, true); release();
    Check(!popup(), "Immediate release after hiding the keyboard retained its popup");
    Text(object, "");
    lv_obj_set_hidden(object, false);
    blank(); holdContinuously("a"); dragChoice(u8"à");
    lv_obj_set_hidden(screen, true); release();
    Check(!popup(), "Immediate release after hiding an ancestor retained its popup");
    Text(object, "");
    lv_obj_set_hidden(screen, false);
    blank(); hold("a");
    auto* hiddenOwnerChoice = Button(popup(), u8"à");
    lv_obj_set_hidden(object, true); Click(hiddenOwnerChoice);
    Check(!popup(), "A stale overlay remained after clicking while its owner was hidden");
    Text(object, "");
    lv_obj_set_hidden(object, false);
    blank(); hold("a");
    auto* hiddenAncestorChoice = Button(popup(), u8"à");
    lv_obj_set_hidden(screen, true); Click(hiddenAncestorChoice);
    Check(!popup(), "A stale overlay remained after clicking while an ancestor was hidden");
    Text(object, "");
    lv_obj_set_hidden(screen, false);
    blank(); holdContinuously("a"); dragChoice(u8"à");
    lv_obj_set_hidden(object, true); Advance(60); release();
    Check(!popup(), "Hiding the keyboard left a drag selection active"); Text(object, "");
    lv_obj_set_hidden(object, false);
    hold("a");
    lv_obj_set_hidden(screen, true); Advance(60);
    Check(!popup(), "Hiding an ancestor left an active variant row");
    lv_obj_set_hidden(screen, false);

    // A second widget's language and lifetime must not touch the first popup.
    hold("a");
    auto* independent = lv_advanced_keyboard_create(screen);
    Check(independent, "Cannot create independent keyboard");
    Check(lv_advanced_keyboard_set_language(independent, "de-DE"), "Cannot set independent keyboard language");
    Check(popup(), "Changing another widget dismissed this widget's variants");
    lv_obj_delete(independent);
    Check(popup(), "Deleting another widget dismissed this widget's variants");
    choose(u8"à"); Text(object, u8"à");
    blank(); holdContinuously("a");
    auto* independentHover = dragChoice(u8"á");
    independent = lv_advanced_keyboard_create(screen);
    Check(independent, "Cannot create independent keyboard during drag");
    Check(lv_advanced_keyboard_set_language(independent, "de-DE"), "Cannot change independent language during drag");
    lv_obj_delete(independent);
    Check(lv_obj_has_state(independentHover, LV_STATE_PRESSED), "Another keyboard's lifetime cleared the drag highlight");
    finishDrag(u8"á");

    // A narrow display wraps long Pinyin choices into multiple rows.
    Check(lv_advanced_keyboard_set_language(object, "zh-CN"), "Cannot select Pinyin for scroll fixture");
    lv_display_set_resolution(display, 320, 480);
    lv_obj_set_width(object, 320); lv_obj_set_width(field, 300); lv_obj_update_layout(screen);
    blank(); hold("u");
    auto* row = popup();
    auto* last = Button(row, u8"ǜ");
    Check(last, "Long variant row lost its last tone mark");
    auto* first = lv_obj_get_child(row, 0);
    lv_area_t firstBounds{}, lastBounds{};
    lv_obj_get_coords(first, &firstBounds); lv_obj_get_coords(last, &lastBounds);
    Check(lastBounds.y1 > firstBounds.y1, "Long variant choices did not wrap into multiple rows");
    Check(lv_obj_get_scroll_x(row) == 0, "Wrapped variants still require horizontal scrolling");
    choose(u8"ǜ"); Text(object, u8"ǜ");
    blank(); holdContinuously("u");
    auto* earlierRowChoice = dragChoice(u8"ù");
    dragChoice(u8"ǜ");
    Check(!lv_obj_has_state(earlierRowChoice, LV_STATE_PRESSED), "Moving between wrapped rows retained the earlier selection");
    finishDrag(u8"ǜ");
    lv_display_set_resolution(display, 640, 480);
    lv_obj_set_width(object, 640); lv_obj_set_width(field, 600); lv_obj_update_layout(screen);

    Check(lv_advanced_keyboard_set_language(object, "en-US"), "Cannot restore English for deletion fixture");
    holdContinuously("a"); dragChoice(u8"à");
    lv_obj_delete(object);
    release();
    Check(!popup(), "Deleting a keyboard left its variant overlay behind");
    Advance(1200);
    object = lv_advanced_keyboard_create(screen);
    lv_obj_set_pos(object, 0, 240); lv_obj_set_size(object, 640, 240);
    Check(lv_advanced_keyboard_set_textarea(object, field), "Cannot attach pending-hold deletion fixture");
    keyboard = lv_advanced_keyboard_get_keyboard(object);
    press(Button(keyboard, "a")); Advance(400);
    lv_obj_delete(object); release(); Advance(1200);
    Check(!popup(), "Deleting a pending hold created a late popup");
    lv_screen_load(previous); lv_obj_delete(screen);
}

}

static void TestProjectKeyboard(const std::filesystem::path& directory)
{
    const auto defaults=ProjectKeyboard::Normalize(json::object());
    Check(!defaults.at("enabled").get<bool>(),"Project keyboard was enabled by default");
    for(const auto& invalid:std::vector<json>{{{"enabled",1}},{{"height",0}},{{"height",-1}},{{"height",32768}},{{"language","unknown"}}}) {
        bool rejected=false;try {ProjectKeyboard::Normalize(invalid);}catch(const std::exception&){rejected=true;}
        Check(rejected,"Invalid project keyboard settings were accepted");
    }
    Check(!ProjectKeyboard::Normalize({{"height",nullptr}}).contains("height"),"Automatic keyboard height did not normalize");
    json source={{"advancedKeyboard",{{"enabled",true},{"trieId",""}}},{"canvas",{{"width",320},{"height",240}}},
        {"variables",json::array()},{"obj",json::array({
            {{"oid",0},{"par",-1},{"base",{{"coords",{{"x",10},{"y",10},{"width",140},{"height",40}}}}},{"textarea",{{"text",""},{"oneLine",true},{"inputMode","uppercase"}}}},
            {{"oid",4},{"par",-1},{"base",{{"coords",{{"x",170},{"y",10},{"width",140},{"height",40}}}}},{"textarea",{{"text",""},{"oneLine",true},{"inputMode","numeric"}}}}
        })}};
    const auto original=source;
    for(const auto& restored:{Protobuf::Decode(Protobuf::Encode(EntityKind::Project,"project",original)).data,
        Xml::Decode(Xml::Encode(EntityKind::Project,"project",original)).data})
        Check(restored.at("advancedKeyboard")==original.at("advancedKeyboard") && restored.at("obj").size()==2,"Project keyboard authoring options did not round trip without widgets");
    auto device=ProjectScreens::Device(source);
    Check(source==original && device["obj"].size()==3,"Screen materialization mutated authoring or missed its keyboard");
    const auto keyboardId=device["obj"].back().at("oid").get<int>();
    Check(keyboardId!=0 && keyboardId!=4 && ProjectKeyboard::Managed(device["obj"].back()),"Managed keyboard marker or unique ID missing");
    Check(device["obj"].back()["base"]["coords"]["height"]==144,"Default screen keyboard obscures the entire small screen");
    auto repeated=device;ProjectKeyboard::Materialize(repeated);Check(repeated==device,"Screen keyboard materialization is not idempotent");
    auto legacy=original;legacy["obj"].push_back({{"oid",7},{"par",-1},{"base",{{"nativeStyles",json::array()}}},{"advancedKeyboard",{{"language","de-DE"},{"trieId",""}}}});
    auto upgraded=legacy;ProjectKeyboard::Materialize(upgraded);
    Check(upgraded["obj"].size()==3 && upgraded["obj"].back()["oid"]==7 && upgraded["obj"].back()["advancedKeyboard"]["language"]=="en-US","Screen settings did not replace the legacy keyboard once");
    legacy["advancedKeyboard"]["enabled"]=false;ProjectKeyboard::Materialize(legacy);
    Check(legacy["obj"].size()==2,"Disabled screen retained a legacy runtime keyboard");
    auto* oldGroup=lv_group_get_default();auto* group=lv_group_create();lv_group_set_default(group);
    auto page=original;page.erase("advancedKeyboard");
    auto other=page;other["canvas"]["height"]=320;
    json multi={{"advancedKeyboard",original.at("advancedKeyboard")},{"activeScreen",0},{"variables",json::array()},
        {"screens",json::array({page,other})}};
    const auto bytes=Protobuf::Encode(EntityKind::Project,"project",multi);
    lvgl_runtime_t* runtime=nullptr;char error[512]{};
    Check(lvgl_runtime_load_memory_ex(bytes.data(),bytes.size(),nullptr,LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE,&runtime,error,sizeof error),error);
    auto* keyboard=lvgl_runtime_object(runtime,0,keyboardId);auto* upper=lvgl_runtime_object(runtime,0,0);auto* numeric=lvgl_runtime_object(runtime,0,4);
    Check(keyboard && lv_obj_is_hidden(keyboard) && !lv_advanced_keyboard_get_textarea(keyboard),"Default runtime focus opened the screen keyboard before editing");
    lv_obj_send_event(upper,LV_EVENT_CLICKED,nullptr);Advance(40);
    Check(!lv_obj_is_hidden(keyboard) && lv_advanced_keyboard_get_textarea(keyboard)==upper,"Editing did not open the screen keyboard");
    auto* keys=lv_advanced_keyboard_get_keyboard(keyboard);
    Check(Button(keys,"A") && !Button(keys,"a"),"Screen keyboard did not respect uppercase target");
    Tap(keys,"A");Check(std::string(lv_textarea_get_text(upper))=="A","Screen keyboard did not type into uppercase target");
    lv_obj_send_event(numeric,LV_EVENT_CLICKED,nullptr);Advance(40);
    Check(lv_advanced_keyboard_get_textarea(keyboard)==numeric && Button(keys,"1") && !Button(keys,"A"),"Screen keyboard did not switch numeric target/layout");
    Tap(keys,"1");Tap(keys,".");Tap(keys,"2");Check(std::string(lv_textarea_get_text(numeric))=="1.2","Numeric screen keyboard typed into the wrong field");
    Tap(lv_obj_get_child(keyboard,0),"Enter");Check(lv_obj_is_hidden(keyboard),"Enter did not dismiss the screen keyboard");
    lv_obj_send_event(upper,LV_EVENT_CLICKED,nullptr);Check(!lv_obj_is_hidden(keyboard),"Dismissed screen keyboard did not reopen");
    lv_advanced_keyboard_close(keyboard);
    Check(lvgl_runtime_show_screen(runtime,1),lvgl_runtime_error(runtime));
    auto* nextKeyboard=lvgl_runtime_object(runtime,1,keyboardId);
    Check(nextKeyboard && lv_obj_is_hidden(nextKeyboard),"Project keyboard was not created hidden on the next screen");
    auto* nextField=lvgl_runtime_object(runtime,1,0);lv_obj_send_event(nextField,LV_EVENT_CLICKED,nullptr);
    Check(lv_advanced_keyboard_get_textarea(nextKeyboard)==nextField && !lv_obj_is_hidden(nextKeyboard),"Project keyboard did not bind the next screen's own field");
    Check(lvgl_runtime_show_screen(runtime,0),lvgl_runtime_error(runtime));
    keyboard=lvgl_runtime_object(runtime,0,keyboardId);
    Check(keyboard && lv_obj_is_hidden(keyboard),"Lazy screen recreation opened its keyboard");
    lvgl_runtime_destroy(runtime);lv_group_set_default(oldGroup);lv_group_delete(group);
    CCodeGenerator generator;Check(generator.LoadProject(original),generator.GetLastError());
    const auto output=directory/"screen-keyboard";Check(generator.ExportProject(output.u8string()),generator.GetLastError());
    std::ifstream generated(output/"ui.c");const std::string code((std::istreambuf_iterator<char>(generated)),{});
    Check(code.find("lv_advanced_keyboard_bind_textarea")!=std::string::npos && code.find("lv_advanced_keyboard_close")!=std::string::npos,"C screen export omitted binding or initial hiding");
    XmlCodeGenerator xml;Check(xml.LoadProject(original) && xml.ExportProject((output/"screen.xml").u8string()),xml.GetLastError());
    std::ifstream xmlFile(output/"screen.xml");const std::string xmlText((std::istreambuf_iterator<char>(xmlFile)),{});
    Check(ProjectKeyboard::Managed(Xml::Decode(xmlText).data["obj"].back()),"XML export omitted its managed keyboard");
    MicroPythonCodeGenerator python;json summary;
    Check(python.LoadProject(original) && !python.Preflight(summary) && python.GetLastError().find("Advanced Keyboard")!=std::string::npos,"MicroPython silently omitted an unsupported enabled keyboard");
    auto disabled=original;disabled["advancedKeyboard"]={{"enabled",false},{"trieId","missing-unused-model"}};
    Check(generator.LoadProject(disabled) && generator.Preflight(summary),generator.GetLastError());
    Check(summary.at("requirements").get<std::string>().find("ADVANCED_KEYBOARD")==std::string::npos,"Disabled screen retained keyboard requirements");
    Check(python.LoadProject(disabled) && python.Preflight(summary),python.GetLastError());
    const auto model=ScriptPredictionFixture();std::string modelId;
    auto catalog=ProjectTries::Import(json::array(),"selected-project-model",model.data(),model.size(),&modelId);
    auto unused=catalog[0];unused["id"]="unused-project-model";unused["name"]="Unused model";
    catalog.insert(catalog.begin(),unused);
    auto modeled=original;modeled["tries"]=catalog;modeled["advancedKeyboard"]["trieId"]=modelId;
    const auto modeledBytes=Protobuf::Encode(EntityKind::Project,"project",modeled);
    for(const uint32_t flags:{uint32_t(0),uint32_t(LVGL_RUNTIME_BORROW_ASSETS)}) {
        runtime=nullptr;
        Check(lvgl_runtime_load_memory_ex(modeledBytes.data(),modeledBytes.size(),nullptr,flags,&runtime,error,sizeof error),error);
        auto* modeledKeyboard=lvgl_runtime_object(runtime,0,keyboardId);
        Check(modeledKeyboard && lv_advanced_keyboard_get_predictor(modeledKeyboard),"Pruning an unused leading model broke the selected borrowed protobuf asset");
        lvgl_runtime_destroy(runtime);
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
        Check(lv_advanced_keyboard_bound_textarea_count(object) == 2 && lv_obj_is_hidden(object), "Unfocused keyboard should be hidden and bound only to authored fields");
        const auto unfocused = ObjectSerializer::SerializeObject(object);
        Click(editor);
        Check(lv_advanced_keyboard_get_textarea(object) == editor && !lv_obj_is_hidden(object), "Focusing an external field did not activate keyboard");
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
        Check(lv_obj_is_hidden(object) && !lv_advanced_keyboard_get_textarea(object) && ready == 1 && fieldReady == 1,
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
        Check(lv_obj_get_child_count(suggestions) >= 4 && lv_obj_get_width(lv_obj_get_child(suggestions, 3)) >= 32 && lv_obj_get_height(lv_obj_get_child(suggestions, 3)) >= 32,
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
        Check(!lv_obj_is_hidden(object) && lv_advanced_keyboard_get_textarea(object) == second, "Outside click dismissed the persistent keyboard");
        Check(AdvancedKeyboard::Normalize({{"hideOnDefocus", true}}).at("hideOnDefocus") == false, "Earlier hide-on-defocus records were not upgraded");
        lv_advanced_keyboard_set_hide_on_defocus(object, true);
        Click(second); Click(inspector); Advance(20);
        Check(lv_obj_is_hidden(object), "Explicit low-level legacy hide-on-defocus option stopped working");
        lv_advanced_keyboard_set_hide_on_defocus(object, false);
        Click(editor); lv_textarea_set_cursor_pos(editor, LV_TEXTAREA_CURSOR_LAST);
        Check(!lv_obj_is_hidden(object), "Refocus did not show keyboard");
        lv_obj_delete(second);
        auto* doomed = ObjectSerializer::Create("textarea", previous);
        struct TargetDeletion { bool deleting=false; unsigned styleEvents=0; } targetDeletion;
        lv_obj_add_event_cb(doomed,[](lv_event_t* event) {
            auto& state=*static_cast<TargetDeletion*>(lv_event_get_user_data(event));
            if(lv_event_get_code(event)==LV_EVENT_DELETE)state.deleting=true;
            else if(state.deleting&&lv_event_get_code(event)==LV_EVENT_STYLE_CHANGED)++state.styleEvents;
        },LV_EVENT_ALL,&targetDeletion);
        Check(lv_advanced_keyboard_set_textarea(object, doomed), "Cannot attach temporary field");
        lv_obj_delete(doomed);
        Check(!lv_advanced_keyboard_get_textarea(object) && lv_obj_is_hidden(object), "Deleted target left a dangling keyboard attachment");
        Check(targetDeletion.deleting&&!targetDeletion.styleEvents,"Deleting a keyboard target reentered its application style handlers");
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
        Check(!lv_obj_is_hidden(object), "Programmatic newline closed the keyboard");
        unsigned formCancelled = 0;
        lv_obj_add_event_cb(previous, CountEdits, LV_EVENT_CANCEL, &formCancelled);
        lv_obj_set_event_bubble(object, true);
        Click(lv_obj_get_child(lv_obj_get_child(object, 0), LV_ADVANCED_KEYBOARD_SUGGESTION_COUNT));
        Check(lv_obj_is_hidden(object) && !lv_advanced_keyboard_get_textarea(object) && cancelled == 1 && fieldCancelled == 0 && formCancelled == 0,
              "X failed to close or cancelled the external field/form");
        lv_obj_set_event_bubble(object, false);
        lv_obj_remove_event_cb_with_user_data(previous, CountEdits, &formCancelled);
        Check(std::string(lv_textarea_get_text(editor)) == "first\nsecond", "X changed external text");
        AdvancedKeyboard::BindTargets(previous, std::map<int, lv_obj_t*>{{7, object}, {8, editor}});
        Check(lv_obj_is_hidden(object) && lv_advanced_keyboard_is_dismissed(object), "Automatic rebind immediately reopened a dismissed keyboard");
        Click(editor); lv_obj_add_state(editor, LV_STATE_DISABLED);
        Tap(keyboard, "\xe2\x86\xb5");
        Check(lv_obj_is_hidden(object) && !lv_advanced_keyboard_get_textarea(object) && std::string(lv_textarea_get_text(editor)) == "first\nsecond",
              "Done failed to close a disabled target or altered its text");
        lv_obj_remove_state(editor, LV_STATE_DISABLED);
        Click(editor); lv_advanced_keyboard_set_text(object, "teh");
        lv_obj_add_event_cb(editor, RefocusReady, LV_EVENT_READY, editor);
        uint32_t enter = LV_KEY_ENTER;
        lv_obj_send_event(editor, LV_EVENT_KEY, &enter);
        Check(std::string(lv_textarea_get_text(editor)) == "the" && lv_obj_is_hidden(object) && !lv_advanced_keyboard_get_textarea(object),
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
        TestLongPressVariants();
        TestScriptKeyboards(directory);
        TestScriptPredictions(directory);
        TestProjectKeyboard(directory);
        lv_obj_delete(object); lv_obj_delete(editor); lv_indev_delete(pointer); pointer = nullptr;
        lv_display_delete(display); lv_deinit();
        std::cout << "Advanced Keyboard clicks, shift/lock, symbols, UTF-8, prediction callback, serialization, runtime bindings and C export passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
