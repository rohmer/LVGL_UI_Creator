// Ported from ESPOS components/ngram_trie; see PROVENANCE.md.
#pragma once
#ifndef LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
#define LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION 1
#endif
#ifndef LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE
#define LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
#endif

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace espos::keyboard::prediction {

class Model;

struct AutocorrectResult {
    // Valid until this scratch is reused. The view is not NUL-terminated.
    std::string_view word;
    double probability = 0;
};

// Allocate once per concurrent caller. It can live in PSRAM and need not be zeroed.
class AutocorrectScratch {
public:
    static constexpr std::size_t kMaxInputRunes = 64;
    static constexpr std::size_t kMaxFoldedTokenBytes = 4 * kMaxInputRunes;
    static constexpr std::size_t kMaxResultBytes = 4 * (kMaxInputRunes + 3);

private:
    static constexpr std::size_t kMaxDepth = kMaxInputRunes + 3;
    static constexpr std::size_t kRowWidth = 8;

    struct Frame {
        std::uint32_t next_child;
        std::uint16_t children_left;
        std::uint8_t depth;
    };

    double rows[(kMaxDepth + 1) * kRowWidth];
    bool swap_can_cross[kMaxDepth + 1];
    std::uint32_t last_rune[kMaxDepth + 1];
    std::uint16_t rune_end[kMaxDepth + 1];
    std::uint32_t observed[kMaxInputRunes];
    Frame frames[kMaxResultBytes];
    char path[kMaxResultBytes];
    char best_word[kMaxResultBytes];
    char typed[kMaxFoldedTokenBytes];
    char previous[kMaxFoldedTokenBytes];
    char older[kMaxFoldedTokenBytes];

    friend struct AutocorrectSearch;
};

// Fold a complete UTF-8 token using the same data as the C# corpus generator.
// Returns false for invalid UTF-8 or insufficient space. Input and output must not overlap.
bool FoldUtf8(std::string_view input, char* destination, std::size_t capacity, std::string_view& folded) noexcept;

// Inputs are whole tokens already folded as in the trie, with context from the same
// sentence. The result is set only when a changed word beats keeping the typed word.
// Its probability includes every eligible candidate and the keep-typed option.
bool BestAutocorrection(const Model& model, AutocorrectScratch& scratch, std::string_view folded_typed,
                        AutocorrectResult& result, std::string_view folded_previous = {},
                        std::string_view folded_older = {}) noexcept;

// Fold typed and contextual tokens into scratch before scoring. Each folded token
// must fit kMaxFoldedTokenBytes. The returned word remains folded for the caller to display.
bool BestAutocorrectionUtf8(const Model& model, AutocorrectScratch& scratch, std::string_view typed,
                            AutocorrectResult& result, std::string_view previous = {},
                            std::string_view older = {}) noexcept;

}  // namespace espos::keyboard::prediction
