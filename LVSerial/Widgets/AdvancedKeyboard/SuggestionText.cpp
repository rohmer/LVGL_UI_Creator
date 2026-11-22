#include "SuggestionText.h"

#include <array>
#include <cstring>
#include <limits>

#include "Utf8Fold.h"
#include "espos_keyboard/Prediction.h"

namespace espos::keyboard::detail {
namespace {

struct Rune {
    std::uint32_t value = 0;
    std::size_t width = 0;
};

bool continuation(unsigned char byte)
{
    return (byte & 0xc0) == 0x80;
}

Rune next_rune(std::string_view text, std::size_t position)
{
    if (position >= text.size()) return {};
    const auto lead = static_cast<unsigned char>(text[position]);
    if (lead < 0x80) return {lead, 1};
    if (lead < 0xc2 || lead > 0xf4) return {0xfffd, 1};

    const std::size_t width = lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
    if (text.size() - position < width) return {0xfffd, 1};
    std::uint32_t value = lead & (0x7f >> width);
    for (std::size_t index = 1; index < width; ++index) {
        const auto byte = static_cast<unsigned char>(text[position + index]);
        if (!continuation(byte)) return {0xfffd, 1};
        value = (value << 6) | (byte & 0x3f);
    }
    if ((width == 3 && value < 0x800) || (width == 4 && value < 0x10000) ||
        (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff) return {0xfffd, 1};
    return {value, width};
}

Rune previous_rune(std::string_view text, std::size_t position)
{
    if (position == 0) return {};
    std::size_t start = position - 1;
    while (start > 0 && position - start < 4 && continuation(text[start])) --start;
    const auto rune = next_rune(text.substr(0, position), start);
    return rune.width == position - start ? rune : Rune{0xfffd, 1};
}

std::size_t byte_at(std::string_view text, std::uint32_t character)
{
    std::size_t position = 0;
    for (std::uint32_t index = 0; index < character && position < text.size(); ++index)
        position += next_rune(text, position).width;
    return position;
}

std::size_t character_count(std::string_view text)
{
    std::size_t count = 0;
    for (std::size_t position = 0; position < text.size(); ++count)
        position += next_rune(text, position).width;
    return count;
}

bool upper(std::uint32_t value)
{
    return (value >= 'A' && value <= 'Z') ||
           (value >= 0xc0 && value <= 0xde && value != 0xd7) || value == 0x178 ||
           (value >= 0x391 && value <= 0x3ab && value != 0x3a2) || prediction::IsCyrillicUpper(value);
}

bool lower(std::uint32_t value)
{
    return (value >= 'a' && value <= 'z') ||
           (value >= 0xdf && value <= 0xff && value != 0xf7) ||
           (value >= 0x3b1 && value <= 0x3cb) || prediction::IsCyrillicLower(value);
}

bool is_word(std::uint32_t value)
{
    return prediction::IsWordScalar(value);
}
bool mark(std::uint32_t value) { return prediction::IsMarkScalar(value); }
bool joiner(std::uint32_t value) { return value == 0x200c || value == 0x200d; }

bool apostrophe(std::uint32_t value)
{
    return value == '\'' || value == 0x2019;
}

bool sentence_separator(std::uint32_t value)
{
    return value == '.' || value == '!' || value == '?' || value == '\r' || value == '\n' ||
           value == 0x2028 || value == 0x2029 || value == 0x61f || value == 0x6d4 ||
           value == 0x964 || value == 0x965 || value == 0x3002 || value == 0xff01 || value == 0xff1f;
}

std::size_t word_start(std::string_view text, std::size_t position, bool hanTokens)
{
    const auto end = position;
    while (position > 0) {
        const auto rune = previous_rune(text, position);
        if (hanTokens && prediction::IsHanScalar(rune.value)) break;
        const auto before = previous_rune(text, position - rune.width);
        if (!is_word(rune.value) && !mark(rune.value) &&
            !(apostrophe(rune.value) && is_word(before.value)) &&
            !(joiner(rune.value) && (is_word(before.value) || mark(before.value)))) break;
        position -= rune.width;
    }
    // Marks and joiners may continue a word, but never start a token alone.
    while (position < end && !is_word(next_rune(text, position).value))
        position += next_rune(text, position).width;
    return position;
}

std::string_view previous_word(std::string_view text, std::size_t& position, bool hanTokens)
{
    while (position > 0) {
        const auto rune = previous_rune(text, position);
        if (hanTokens && prediction::IsHanScalar(rune.value)) {
            position -= rune.width;
            return text.substr(position, rune.width);
        }
        if (is_word(rune.value) || mark(rune.value)) {
            const auto end = position;
            const auto start = word_start(text, position, hanTokens);
            if (start < end) { position = start; return text.substr(start, end - start); }
        }
        if (sentence_separator(rune.value)) {
            position = 0;
            break;
        }
        position -= rune.width;
    }
    return {};
}

bool all_caps(std::string_view text)
{
    unsigned capitals = 0;
    for (std::size_t position = 0; position < text.size();) {
        const auto rune = next_rune(text, position);
        if (lower(rune.value)) return false;
        if (upper(rune.value)) ++capitals;
        position += rune.width;
    }
    return capitals > 1;
}

// The English profile primarily uses ASCII; keep accented Latin capitals at
// the same byte width and preserve UTF-8 that has no same-width mapping here.
void capitalize(char* output, std::string_view text, bool entire_word)
{
    for (std::size_t position = 0; position < text.size();) {
        const auto rune = next_rune(text, position);
        auto value = rune.value;
        if ((value >= 'a' && value <= 'z') || (value >= 0xe0 && value <= 0xfe && value != 0xf7) ||
            (value >= 0x3b1 && value <= 0x3cb)) value -= 32;
        else if (value == 0xff) value = 0x178;
        if (rune.value == 0x3c2) value = 0x3a3;
        value = prediction::UpperCyrillic(value);
        const auto width = value < 0x80 ? 1u : value < 0x800 ? 2u : value < 0x10000 ? 3u : 4u;
        if (value != rune.value && width == rune.width) {
            if (rune.width == 1) output[position] = static_cast<char>(value);
            else if (rune.width == 2) {
                output[position] = static_cast<char>(0xc0 | (value >> 6));
                output[position + 1] = static_cast<char>(0x80 | (value & 0x3f));
            } else if (rune.width == 3) {
                output[position] = static_cast<char>(0xe0 | (value >> 12));
                output[position + 1] = static_cast<char>(0x80 | ((value >> 6) & 0x3f));
                output[position + 2] = static_cast<char>(0x80 | (value & 0x3f));
            }
        }
        if (!entire_word) return;
        position += rune.width;
    }
}

bool is_selected(lv_obj_t* textarea)
{
    auto* label = lv_textarea_get_label(textarea);
    const auto start = lv_label_get_text_selection_start(label);
    const auto end = lv_label_get_text_selection_end(label);
    return start != LV_LABEL_TEXT_SELECTION_OFF && end != LV_LABEL_TEXT_SELECTION_OFF && start != end;
}

}  // namespace

SuggestionText capture_suggestion_text(lv_obj_t* textarea, bool hanTokens)
{
    SuggestionText input{};
    if (textarea == nullptr || lv_textarea_get_password_mode(textarea)) {
        input.selected = true;
        return input;
    }
    const char* text = lv_textarea_get_text(textarea);
    if (text == nullptr) {
        input.selected = true;
        return input;
    }
    input.text = text;
    input.caret = lv_textarea_get_cursor_pos(textarea);
    input.selected = is_selected(textarea);
    if (input.selected) return input;

    const auto caret_byte = byte_at(input.text, input.caret);
    input.start = word_start(input.text, caret_byte, hanTokens);
    input.end = caret_byte;
    while (input.end < input.text.size()) {
        const auto rune = next_rune(input.text, input.end);
        if (hanTokens && prediction::IsHanScalar(rune.value)) break;
        const auto next = next_rune(input.text, input.end + rune.width);
        if (!is_word(rune.value) && !(mark(rune.value) && input.end > input.start) &&
            !(apostrophe(rune.value) && input.end > input.start && is_word(next.value)) &&
            !(joiner(rune.value) && input.end > input.start && (is_word(next.value) || mark(next.value)))) break;
        input.end += rune.width;
    }
    input.prefix = input.text.substr(input.start, caret_byte - input.start);
    auto context = input.start;
    input.previous = previous_word(input.text, context, hanTokens);
    input.older = previous_word(input.text, context, hanTokens);
    return input;
}

std::size_t display_suggestion(const SuggestionText& input, std::string_view raw,
                               char* output, std::size_t capacity)
{
    if (output == nullptr || capacity == 0) return 0;
    output[0] = '\0';
    if (input.selected || raw.empty() || raw.size() > prediction::kSuggestionBytes) return 0;

    std::array<char, prediction::kSuggestionBytes> folded_storage;
    std::string_view folded;
    if (!prediction::FoldUtf8(input.prefix, folded_storage.data(), folded_storage.size(), folded)) return 0;
    const bool completion = !folded.empty() && raw.substr(0, folded.size()) == folded;
    const auto preserved = completion ? input.prefix : std::string_view{};
    const auto suffix = completion ? raw.substr(folded.size()) : raw;
    const auto size = preserved.size() + suffix.size();
    if (size >= capacity) return 0;
    if (!preserved.empty()) std::memcpy(output, preserved.data(), preserved.size());
    if (!suffix.empty()) std::memcpy(output + preserved.size(), suffix.data(), suffix.size());
    output[size] = '\0';

    if (all_caps(input.prefix)) capitalize(output + preserved.size(), suffix, true);
    else if (preserved.empty() && (input.prefix.empty() ? input.previous.empty() :
             upper(next_rune(input.prefix, 0).value))) capitalize(output, suffix, false);
    return size;
}

bool accept_suggestion(lv_obj_t* textarea, const SuggestionText& input, std::string_view word, bool add_space, bool hanTokens)
{
    if (textarea == nullptr || word.empty() || input.selected || lv_textarea_get_password_mode(textarea) ||
        lv_textarea_get_text(textarea) != input.text.data() || lv_textarea_get_cursor_pos(textarea) != input.caret ||
        is_selected(textarea) || input.start > input.end || input.end > input.text.size()) return false;

    const auto boundary = [&](std::size_t offset) { return offset == input.text.size() || !continuation(static_cast<unsigned char>(input.text[offset])); };
    const auto caret_byte = byte_at(input.text, input.caret);
    if (!boundary(input.start) || !boundary(input.end) || input.start > caret_byte || input.end < caret_byte ||
        input.prefix != input.text.substr(input.start, caret_byte - input.start)) return false;
    for (std::size_t index = 0; index < word.size();) {
        const auto rune = next_rune(word, index);
        if (!rune.value || (rune.value == 0xfffd && rune.width == 1)) return false;
        index += rune.width;
    }
    add_space = add_space && !hanTokens;
    const auto tail = input.text.substr(input.end);
    const bool append_space = add_space && tail.empty();
    const bool advance_space = add_space && !tail.empty() && (tail.front() == ' ' || tail.front() == '\t');
    const auto preserved = input.start + tail.size();
    constexpr auto max_size = std::numeric_limits<std::size_t>::max();
    const std::size_t overhead = append_space ? 2 : 1;
    if (preserved > max_size - overhead || word.size() > max_size - overhead - preserved) return false;
    const auto size = preserved + word.size() + static_cast<std::size_t>(append_space);
    const auto prefix_length = character_count(input.prefix);
    if (prefix_length > input.caret) return false;
    const auto caret = input.caret - prefix_length + character_count(word) + (append_space || advance_space);
    if (caret > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) return false;

    auto* replacement = static_cast<char*>(lv_malloc(size + 1));
    if (replacement == nullptr) return false;
    if (input.start != 0) std::memcpy(replacement, input.text.data(), input.start);
    std::memcpy(replacement + input.start, word.data(), word.size());
    if (append_space) replacement[size - 1] = ' ';
    else std::memcpy(replacement + input.start + word.size(), tail.data(), tail.size());
    replacement[size] = '\0';
    lv_textarea_set_text(textarea, replacement);
    lv_free(replacement);
    if (!lv_obj_is_in_widget_tree(textarea)) return false;
    lv_textarea_set_cursor_pos(textarea, static_cast<std::int32_t>(caret));
    return true;
}

}  // namespace espos::keyboard::detail
