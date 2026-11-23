#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "espos_keyboard/Key.h"
#include "espos_keyboard/Prediction.h"

namespace espos::keyboard::view {
using SuggestionLookup = std::size_t (*)(void* context, std::string_view prefix,
    std::string_view previous, std::string_view older, prediction::Suggestion* output) noexcept;
[[nodiscard]] lv_obj_t* create_en_us_typing_view(lv_obj_t* parent, std::int32_t width, std::int32_t height,
    SuggestionLookup lookup, void* context, const KeyStyle& style = {});
[[nodiscard]] lv_obj_t* typing_view_editor(lv_obj_t* view);
[[nodiscard]] lv_obj_t* typing_view_keyboard(lv_obj_t* view);
bool set_typing_view_editor(lv_obj_t* view, lv_obj_t* editor);
bool set_typing_view_language(lv_obj_t* view, const char* id);
void set_typing_view_right_to_left(lv_obj_t* view, bool enabled);
bool typing_view_right_to_left(lv_obj_t* view);
void reset_typing_view_composition(lv_obj_t* view);
bool authored_typing_view_style(lv_obj_t* editor, lv_style_prop_t property, bool& exists, lv_style_value_t& value);
using WordBoundaryHandler = void (*)(void* context, lv_obj_t* editor);
void set_typing_view_word_boundary_handler(lv_obj_t* view, WordBoundaryHandler handler, void* context);
using DismissHandler = void (*)(void* context);
void set_typing_view_dismiss_handler(lv_obj_t* view, DismissHandler handler, void* context);
void set_typing_view_lookup(lv_obj_t* view, SuggestionLookup lookup, void* context);
}
