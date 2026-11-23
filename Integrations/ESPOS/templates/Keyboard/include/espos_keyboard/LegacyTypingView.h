#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include "espos_keyboard/Key.h"
#include "ngram_trie/prefix.h"
#include "lv_advanced_keyboard.h"

// Board-demo adapter. New integrations can use lv_advanced_keyboard_* directly
// to bind one keyboard to any number of externally owned textareas.
namespace espos::keyboard {
using SuggestionLookup = std::size_t (*)(void*, std::string_view, std::string_view,
    std::string_view, ngram_trie::Suggestion*) noexcept;
[[nodiscard]] lv_display_t* create_default_typing_display();
[[nodiscard]] lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, std::int32_t width, std::int32_t height,
    SuggestionLookup lookup, void* context, const KeyStyle& style = {});
[[nodiscard]] lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, SuggestionLookup lookup,
    void* context, const KeyStyle& style = {});
[[nodiscard]] lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, std::int32_t width,
    std::int32_t height, const KeyStyle& style = {});
[[nodiscard]] lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, const KeyStyle& style = {});
bool set_typing_view_orientation(lv_obj_t* view, lv_display_rotation_t rotation);
[[nodiscard]] lv_obj_t* typing_view_editor(lv_obj_t* view);
[[nodiscard]] lv_obj_t* typing_view_keyboard(lv_obj_t* view);
// The actual C facade object, for additional targets and prediction controls.
[[nodiscard]] lv_obj_t* typing_view_advanced_keyboard(lv_obj_t* view);
bool set_typing_view_editor(lv_obj_t* view, lv_obj_t* editor);
bool set_typing_view_language(lv_obj_t* view, const char* language);
const char* typing_view_language(lv_obj_t* view);
bool set_typing_view_right_to_left(lv_obj_t* view, bool enabled);
bool typing_view_right_to_left(lv_obj_t* view);
bool set_typing_view_trie(lv_obj_t* view, const char* language, std::uint32_t size_kib);
// Transfers ownership only on success. The optional lease retains mapped flash
// bytes through predictor destruction, including the no-PSRAM fallback.
bool set_typing_view_owned_predictor(lv_obj_t* view, lv_advanced_keyboard_predictor_t* predictor,
    std::shared_ptr<const void> model_owner = {});
}
