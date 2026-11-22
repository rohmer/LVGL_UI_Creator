#pragma once

#include <cstddef>

#include "espos_keyboard/Key.h"

namespace espos::keyboard {

enum class KeyInputKind { Insert, Backspace, Enter };

struct KeyInput {
    KeyInputKind kind;
    const char* text;  // UTF-8 for Insert, otherwise nullptr.
};

// Creates an en-US keyboard owned by parent. The parent must belong to an
// initialized LVGL display. The optional style font must outlive the keyboard.
// LV_EVENT_VALUE_CHANGED carries a KeyInput valid for the duration of the event.
// Call on the LVGL thread or while holding the same lock used by the UI.
[[nodiscard]] lv_obj_t* create_en_us_keyboard(lv_obj_t* parent, std::int32_t width,
                                              std::int32_t height, const KeyStyle& style = {});

// Consumes one-shot Shift after the editor accepts a suggestion.
void commit_en_us_keyboard_text(lv_obj_t* keyboard);

// Layout changes keep the external editor untouched and reset Shift/symbol pages.
// Language IDs are stable BCP 47 tags; the vocabulary trie is configured separately.
bool set_keyboard_language(lv_obj_t* keyboard, const char* id);
const char* keyboard_language(lv_obj_t* keyboard);
std::size_t keyboard_language_count();
const char* keyboard_language_id(std::size_t index);
const char* keyboard_language_name(std::size_t index);
// Shared static script fonts include the original key-symbol font as fallback.
const lv_font_t* keyboard_language_font(const char* id);
bool keyboard_language_right_to_left(const char* id);

}  // namespace espos::keyboard
