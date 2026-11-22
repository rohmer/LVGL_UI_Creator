#pragma once
#include <array>
#include <cstddef>
#include <cstring>
#ifndef LV_ADVANCED_KEYBOARD_USE_PINYIN
#define LV_ADVANCED_KEYBOARD_USE_PINYIN 1
#endif

namespace espos::keyboard::detail {
#if LV_ADVANCED_KEYBOARD_USE_PINYIN
struct PinyinEntry { const char* spelling; const char* candidates; };
inline constexpr PinyinEntry pinyin_dictionary[] = {
#include "PinyinData.inc"
};

// Single-syllable character composition. All shared data stays in read-only
// storage; each keyboard owns only its spelling and its candidate page.
class PinyinComposition {
    std::array<char, 8> spelling{};
    std::size_t length = 0;
    const char* Match() const {
        if (!length) return "";
        std::size_t first = 0, last = sizeof(pinyin_dictionary) / sizeof(*pinyin_dictionary);
        while (first < last) {
            const auto middle = first + (last - first) / 2;
            const auto order = std::strcmp(pinyin_dictionary[middle].spelling, spelling.data());
            if (order < 0) first = middle + 1; else last = middle;
        }
        return first < sizeof(pinyin_dictionary) / sizeof(*pinyin_dictionary) &&
            !std::strcmp(pinyin_dictionary[first].spelling, spelling.data()) ? pinyin_dictionary[first].candidates : "";
    }
    static std::size_t Width(unsigned char first) { return first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4; }
public:
    bool Append(char letter) {
        if (letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter + ('a' - 'A'));
        if (letter < 'a' || letter > 'z' || length + 1 >= spelling.size()) return false;
        spelling[length++] = letter; spelling[length] = '\0'; return true;
    }
    bool Backspace() { if (!length) return false; spelling[--length] = '\0'; return true; }
    void Clear() { length = 0; spelling[0] = '\0'; }
    const char* Spelling() const { return spelling.data(); }
    std::size_t CandidateCount() const { std::size_t count = 0; for (auto* text = Match(); *text; text += Width(static_cast<unsigned char>(*text))) ++count; return count; }
    bool Candidate(std::size_t index, char output[5]) const {
        output[0] = '\0'; auto* text = Match();
        while (*text && index) { text += Width(static_cast<unsigned char>(*text)); --index; }
        if (!*text) return false;
        const auto width = Width(static_cast<unsigned char>(*text)); std::memcpy(output, text, width); output[width] = '\0'; return true;
    }
};
#else
class PinyinComposition {
public:
    bool Append(char) { return false; }
    bool Backspace() { return false; }
    void Clear() {}
    const char* Spelling() const { return ""; }
    std::size_t CandidateCount() const { return 0; }
    bool Candidate(std::size_t, char output[5]) const { output[0] = '\0'; return false; }
};
#endif
}
