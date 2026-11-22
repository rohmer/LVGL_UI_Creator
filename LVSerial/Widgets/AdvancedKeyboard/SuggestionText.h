#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "lvgl.h"

namespace espos::keyboard::detail {

// Views borrow the textarea text until it is edited. The replacement range
// uses byte offsets, while the caret uses LVGL's character index.
struct SuggestionText {
    std::string_view text;
    std::string_view prefix;
    std::string_view previous;
    std::string_view older;
    std::size_t start = 0;
    std::size_t end = 0;
    std::uint32_t caret = 0;
    bool selected = false;
};

SuggestionText capture_suggestion_text(lv_obj_t* textarea, bool hanTokens = false);
std::size_t display_suggestion(const SuggestionText& input, std::string_view raw,
                               char* output, std::size_t capacity);
bool accept_suggestion(lv_obj_t* textarea, const SuggestionText& input, std::string_view word,
                       bool add_space = true, bool hanTokens = false);

}  // namespace espos::keyboard::detail
