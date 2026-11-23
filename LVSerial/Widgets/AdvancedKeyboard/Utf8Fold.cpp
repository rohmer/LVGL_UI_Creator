// Ported from ESPOS components/ngram_trie; see PROVENANCE.md.
// The project feature gates live in lv_conf.h. Load them before the portable
// engine's fallback defaults, including in callback-only (no model) profiles.
#if !defined(ESPOS_KEYBOARD_HEADLESS_PREDICTION) || !ESPOS_KEYBOARD_HEADLESS_PREDICTION
#include <lvgl.h>
#endif
#include "Utf8Fold.h"
#include "PredictionAutocorrect.h"

#include "PredictionModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>

namespace espos::keyboard::prediction {
namespace {

constexpr std::uint32_t kExpansionBase = 0x110000;

struct ScalarMapping {
    std::uint32_t scalar;
    std::uint32_t value;
};

#include "UnicodeFold.inc"
struct WordRange { std::uint32_t first, last; };
struct IndicDecomposition { std::uint32_t scalar, first, second; };
#include "UnicodeWordRanges.inc"

enum class Utf8Status { Done, Partial, Invalid };

Utf8Status DecodeUtf8(std::string_view bytes, std::uint32_t& scalar, std::size_t& width) noexcept {
    if (bytes.empty()) return Utf8Status::Partial;
    const auto lead = static_cast<std::uint8_t>(bytes.front());
    if (lead < 0x80) {
        scalar = lead;
        width = 1;
        return Utf8Status::Done;
    }
    if (lead < 0xc2 || lead > 0xf4) return Utf8Status::Invalid;
    width = lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
    scalar = lead & (0x7f >> width);
    for (std::size_t i = 1; i < width; ++i) {
        if (i == bytes.size()) return Utf8Status::Partial;
        const auto next = static_cast<std::uint8_t>(bytes[i]);
        if ((next & 0xc0) != 0x80) return Utf8Status::Invalid;
        if (i == 1 && ((lead == 0xe0 && next < 0xa0) || (lead == 0xed && next >= 0xa0) ||
                       (lead == 0xf0 && next < 0x90) || (lead == 0xf4 && next > 0x8f)))
            return Utf8Status::Invalid;
        scalar = (scalar << 6) | (next & 0x3f);
    }
    return Utf8Status::Done;
}

bool AppendUtf8(std::uint32_t scalar, char* destination, std::size_t capacity, std::size_t& used) noexcept {
    const unsigned width = scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
    if (width > capacity - used) return false;
    if (width == 1) {
        destination[used++] = static_cast<char>(scalar);
        return true;
    }
    for (unsigned i = width - 1; i != 0; --i) {
        destination[used + i] = static_cast<char>(0x80 | (scalar & 0x3f));
        scalar >>= 6;
    }
    destination[used] = static_cast<char>((0xffu << (8 - width)) | scalar);
    used += width;
    return true;
}

template <std::size_t Count>
std::uint32_t Lookup(const ScalarMapping (&table)[Count], std::uint32_t scalar) noexcept {
    const auto* entry = std::lower_bound(std::begin(table), std::end(table), scalar,
        [](const ScalarMapping& item, std::uint32_t target) { return item.scalar < target; });
    return entry != std::end(table) && entry->scalar == scalar ? entry->value : scalar;
}

std::uint32_t AsciiLower(std::uint32_t scalar) noexcept {
    return scalar >= 'A' && scalar <= 'Z' ? scalar + ('a' - 'A') : scalar;
}

template <std::size_t Count>
bool Inside(const WordRange (&ranges)[Count], std::uint32_t scalar) noexcept {
    const auto* next = std::upper_bound(std::begin(ranges), std::end(ranges), scalar,
        [](std::uint32_t value, const WordRange& range) { return value < range.first; });
    return next != std::begin(ranges) && scalar <= (next - 1)->last;
}

std::uint32_t SimpleLower(std::uint32_t scalar) noexcept {
    if (scalar < 0x80) return AsciiLower(scalar);
    const auto override = std::lower_bound(std::begin(kSimpleLowerOverrides), std::end(kSimpleLowerOverrides), scalar,
        [](const ScalarMapping& item, std::uint32_t target) { return item.scalar < target; });
    if (override != std::end(kSimpleLowerOverrides) && override->scalar == scalar) return override->value;
    const std::uint32_t mapping = Lookup(kFoldMappings, scalar);
    return mapping < kExpansionBase ? mapping : scalar;
}

#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
bool IsNumber(std::uint32_t scalar) noexcept {
    if (scalar < 0x80) return scalar >= '0' && scalar <= '9';
    const auto* next = std::upper_bound(std::begin(kNumberRanges), std::end(kNumberRanges), scalar,
        [](std::uint32_t target, const ScalarMapping& range) { return target < range.scalar; });
    return next != std::begin(kNumberRanges) && scalar <= (next - 1)->value;
}

bool IsProtectedWord(std::string_view word) noexcept {
    // The public normalized entry point receives folded tokens, including possessives.
    if (word.size() > 2 && word.substr(word.size() - 2) == "'s") word.remove_suffix(2);
    else if (word.size() > 4 && word.substr(word.size() - 4) == "\xe2\x80\x99s") word.remove_suffix(4);
    return std::binary_search(std::begin(kProtectedWords), std::end(kProtectedWords), word);
}

bool Utf16OrdinalLess(std::string_view left, std::string_view right) noexcept {
    // The desktop uses StringComparer.Ordinal, whose order is by UTF-16 code unit.
    const auto first_unit = [](std::uint32_t scalar) {
        return scalar < 0x10000 ? scalar : 0xd800 + ((scalar - 0x10000) >> 10);
    };
    while (!left.empty() && !right.empty()) {
        std::uint32_t a = 0, b = 0;
        std::size_t a_width = 0, b_width = 0;
        if (DecodeUtf8(left, a, a_width) != Utf8Status::Done || DecodeUtf8(right, b, b_width) != Utf8Status::Done)
            return left < right;
        if (a != b) {
            const auto a_unit = first_unit(a), b_unit = first_unit(b);
            return a_unit == b_unit ? a < b : a_unit < b_unit;
        }
        left.remove_prefix(a_width);
        right.remove_prefix(b_width);
    }
    return left.empty() && !right.empty();
}

constexpr unsigned kAlphabet = 26;
const double kCorrect = -std::log(0.90);
const double kDeletion = -std::log(0.01) - kCorrect;
const double kInsertion = -std::log(0.01 / (2 * kAlphabet)) - kCorrect;
const double kSwap = -std::log(0.02) - 2 * kCorrect;
const double kUnknownSubstitution = -std::log(0.03 / (kAlphabet - 1)) - kCorrect;

constexpr std::array<std::uint32_t, kAlphabet> NeighborMasks() {
    constexpr std::string_view rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    std::array<std::uint32_t, kAlphabet> result{};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < static_cast<int>(rows[row].size()); ++column) {
            auto& mask = result[rows[row][column] - 'a'];
            for (int other_row = std::max(0, row - 1); other_row <= std::min(2, row + 1); ++other_row) {
                for (int other_column = std::max(0, column - 1);
                     other_column <= std::min(static_cast<int>(rows[other_row].size()) - 1, column + 1); ++other_column) {
                    if (row != other_row || column != other_column) mask |= 1u << (rows[other_row][other_column] - 'a');
                }
            }
        }
    }
    return result;
}

// Use the same left-aligned thesis keyboard as the desktop en-US QWERTY profile.
constexpr auto kNeighborMasks = NeighborMasks();

struct KeyCost { double neighbor; double other; };

const auto kKeyCosts = [] {
    std::array<KeyCost, kAlphabet> costs{};
    for (unsigned i = 0; i < kAlphabet; ++i) {
        unsigned count = 0;
        for (auto mask = kNeighborMasks[i]; mask != 0; mask &= mask - 1) ++count;
        costs[i] = {-std::log(0.05 / std::max(1u, count)) - kCorrect,
                    -std::log(0.03 / std::max(1u, kAlphabet - 1 - count)) - kCorrect};
    }
    return costs;
}();

double Substitution(std::uint32_t intended, std::uint32_t observed) noexcept {
    if (intended == observed) return 0;
    unsigned source = intended - 'a', target = observed - 'a';
    if (source >= kAlphabet || target >= kAlphabet) {
        intended = SimpleLower(intended);
        observed = SimpleLower(observed);
        if (intended == observed) return 0;
        source = intended - 'a';
        target = observed - 'a';
        if (source >= kAlphabet) return kUnknownSubstitution;
    }
    const auto& key = kKeyCosts[source];
    return target < kAlphabet && (kNeighborMasks[source] & (1u << target)) != 0 ? key.neighbor : key.other;
}

#endif
}  // namespace

bool IsWordScalar(std::uint32_t scalar) noexcept {
    if (scalar < 0x80) return (scalar >= 'A' && scalar <= 'Z') || (scalar >= 'a' && scalar <= 'z') ||
        (scalar >= '0' && scalar <= '9');
    return Inside(kUnicodeWordRanges, scalar);
}
bool IsMarkScalar(std::uint32_t scalar) noexcept { return scalar >= 0x300 && Inside(kUnicodeMarkRanges, scalar); }
bool IsHanScalar(std::uint32_t scalar) noexcept {
    return (scalar >= 0x3400 && scalar <= 0x4dbf) || (scalar >= 0x4e00 && scalar <= 0x9fff) ||
        (scalar >= 0xf900 && scalar <= 0xfaff) || (scalar >= 0x20000 && scalar <= 0x2ee5f) ||
        (scalar >= 0x2f800 && scalar <= 0x2fa1f) || (scalar >= 0x30000 && scalar <= 0x323af);
}
bool IsCyrillicUpper(std::uint32_t scalar) noexcept { return Lookup(kCyrillicLowerMappings, scalar) != scalar; }
bool IsCyrillicLower(std::uint32_t scalar) noexcept { return Lookup(kCyrillicUpperMappings, scalar) != scalar; }
std::uint32_t UpperCyrillic(std::uint32_t scalar) noexcept { return Lookup(kCyrillicUpperMappings, scalar); }

bool FoldUtf8(std::string_view input, char* destination, std::size_t capacity, std::string_view& folded, bool canonicalIndic) noexcept {
    folded = {};
    if (destination == nullptr && !input.empty()) return false;
    std::size_t used = 0;
    while (!input.empty()) {
        std::uint32_t scalar = 0;
        std::size_t width = 0;
        if (DecodeUtf8(input, scalar, width) != Utf8Status::Done) return false;
        if (canonicalIndic && scalar >= 0x900 && scalar <= 0x9ff) {
            const auto* entry = std::lower_bound(std::begin(kIndicNuktaMappings), std::end(kIndicNuktaMappings), scalar,
                [](const IndicDecomposition& mapping, std::uint32_t value) { return mapping.scalar < value; });
            if (entry != std::end(kIndicNuktaMappings) && entry->scalar == scalar) {
                if (!AppendUtf8(entry->first, destination, capacity, used) || !AppendUtf8(entry->second, destination, capacity, used)) return false;
                input.remove_prefix(width); continue;
            }
        }
        const std::uint32_t mapping = scalar < 0x80 ? AsciiLower(scalar) : Lookup(kFoldMappings, scalar);
        if (mapping < kExpansionBase) {
            if (!AppendUtf8(mapping, destination, capacity, used)) return false;
        } else {
            const auto expansion = mapping - kExpansionBase;
            const auto offset = expansion / 4;
            const auto count = expansion % 4;
            for (std::uint32_t i = 0; i < count; ++i)
                if (!AppendUtf8(kFoldExpansions[offset + i], destination, capacity, used)) return false;
        }
        input.remove_prefix(width);
    }
    if (used != 0) folded = {destination, used};
    return true;
}

bool FoldUtf8(std::string_view input, char* destination, std::size_t capacity, std::string_view& folded) noexcept {
    return FoldUtf8(input, destination, capacity, folded, false);
}

bool PrepareKeyboardQuery(std::string_view input, char* destination, std::size_t capacity,
                          std::string_view& prepared, const char* language, bool completeWord) noexcept {
    const bool greek = language && !std::strcmp(language, "el-GR");
    const bool french = language && !std::strcmp(language, "fr-FR");
    const bool german = language && !std::strcmp(language, "de-DE");
    if (!greek && !french && !german) {
        const bool indic = language && (!std::strcmp(language, "hi-IN") || !std::strcmp(language, "bn-BD"));
        return FoldUtf8(input, destination, capacity, prepared, indic);
    }
    // These vocabulary-filtered models retain lowercase spelling rather than
    // Unicode case-fold expansions: German sharp S and Greek final sigma are
    // distinct stored bytes. Their keyboard keys already emit NFC characters.
    prepared = {};
    if (!destination && !input.empty()) return false;
    std::size_t used = 0, runes = 0;
    while (!input.empty()) {
        std::uint32_t scalar = 0; std::size_t width = 0;
        if (DecodeUtf8(input, scalar, width) != Utf8Status::Done) return false;
        const bool finalSigma = greek && completeWord && runes && width == input.size() && scalar == 0x3a3;
        scalar = finalSigma ? 0x3c2 : SimpleLower(scalar);
        if (french && scalar == 0x2019) scalar = '\'';
        if (!AppendUtf8(scalar, destination, capacity, used)) return false;
        input.remove_prefix(width); ++runes;
    }
    if (used) prepared = {destination, used};
    return true;
}

#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
struct AutocorrectSearch {
    const Model& model;
    AutocorrectScratch& scratch;
    std::string_view typed;
    std::uint32_t bigram = Model::kNotFound;
    std::uint32_t trigram = Model::kNotFound;
    std::size_t best_length = 0;
    double best_score = 0;
    double total_weight = 1;

    std::uint32_t FollowSpace(std::uint32_t node) const noexcept {
        return node == Model::kNotFound ? node : model.FindChild(node, ' ');
    }

    std::uint32_t CountAfter(std::uint32_t prefix, std::string_view word) const noexcept {
        if (prefix == Model::kNotFound) return 0;
        Node node{};
        const auto id = model.FollowFrom(prefix, word);
        return id != Model::kNotFound && model.GetNode(id, node, false) ? node.terminal : 0;
    }

    double Score(std::string_view word, std::uint32_t count, double distance) const noexcept {
        double score = 0.55 * std::log(count) - distance;
        if (const auto count3 = CountAfter(trigram, word); count3 > 0) score += 0.9 * std::log(1.0 + count3);
        else if (const auto count2 = CountAfter(bigram, word); count2 > 0) score += 0.55 * std::log(1.0 + count2);
        return score;
    }

    void Consider(std::string_view word, std::uint32_t count, double distance) noexcept {
        if (word == typed) return;
        const auto score = Score(word, count, distance);
        if (score > best_score) {
            total_weight = total_weight * std::exp(best_score - score) + 1;
            best_score = score;
        } else {
            total_weight += std::exp(score - best_score);
            if (score != best_score || best_length == 0 ||
                !Utf16OrdinalLess(word, {scratch.best_word, best_length})) return;
        }
        std::memcpy(scratch.best_word, word.data(), word.size());
        best_length = word.size();
    }

    bool Run(std::string_view input, AutocorrectResult& result, std::string_view previous, std::string_view older) noexcept {
        result = {};
        if (input.empty() || input.size() > sizeof(scratch.typed) || IsProtectedWord(input)) return false;
        std::memmove(scratch.typed, input.data(), input.size());
        typed = {scratch.typed, input.size()};
        int observed_count = 0;
        auto rest = typed;
        while (!rest.empty()) {
            std::uint32_t scalar = 0;
            std::size_t width = 0;
            if (observed_count == static_cast<int>(AutocorrectScratch::kMaxInputRunes) ||
                DecodeUtf8(rest, scalar, width) != Utf8Status::Done || IsNumber(scalar)) return false;
            scratch.observed[observed_count++] = scalar;
            rest.remove_prefix(width);
        }

        Node root{};
        if (!model.GetNode(static_cast<std::uint32_t>(Root::Unigrams), root, false) || root.child_count == 0)
            return false;
        if (!previous.empty()) bigram = FollowSpace(model.FindPrefix(previous));
        if (!older.empty()) {
            const auto prefix = FollowSpace(model.FindPrefix(older));
            if (prefix != Model::kNotFound) trigram = FollowSpace(model.FollowFrom(prefix, previous));
        }
        Node exact{};
        const auto exact_id = model.FindPrefix(typed, Root::Unigrams);
        if (exact_id != Model::kNotFound && model.GetNode(exact_id, exact, false) && exact.terminal != 0)
            best_score = Score(typed, exact.terminal, 0);

        const double budget = observed_count == 1 ? 7 : observed_count == 2 ? 8.5 : observed_count <= 4 ? 13.5 : 17;
        const int deletions = static_cast<int>(budget / kDeletion);
        const int insertions = static_cast<int>(budget / kInsertion);
        const int longest = observed_count + deletions;
        const int row_width = deletions + insertions + 3;
        const int diagonal = deletions + 1;
        constexpr double infinity = std::numeric_limits<double>::infinity();
        for (int depth = 0; depth <= longest; ++depth) {
            scratch.rows[depth * row_width] = infinity;
            scratch.rows[(depth + 1) * row_width - 1] = infinity;
        }
        for (int i = 0; i <= std::min(observed_count, insertions); ++i) scratch.rows[diagonal + i] = i * kInsertion;

        // Iterate the byte trie without placing a frame per byte on the task stack.
        // Rune positions and DP rows are shared across the continuation bytes of a scalar.
        int length = 0;
        scratch.frames[0] = {root.first_child, root.child_count, 0};
        scratch.rune_end[0] = 0;
        scratch.last_rune[0] = 0;
        scratch.swap_can_cross[0] = kSwap <= budget;
        while (length >= 0) {
            auto& frame = scratch.frames[length];
            if (frame.children_left == 0) {
                --length;
                continue;
            }
            const auto id = frame.next_child++;
            --frame.children_left;
            Node child{};
            if (!model.GetNode(id, child, false)) return false;
            if (child.label == ' ') continue;
            scratch.path[length] = static_cast<char>(child.label);
            const int depth = frame.depth;
            const int rune_start = scratch.rune_end[depth];
            const int bytes = length + 1;
            std::uint32_t scalar = child.label;
            if (length != rune_start || child.label >= 0x80) {
                std::size_t width = 0;
                const auto status = DecodeUtf8({scratch.path + rune_start, static_cast<std::size_t>(bytes - rune_start)}, scalar, width);
                if (status == Utf8Status::Partial && child.child_count != 0 &&
                    bytes < static_cast<int>(AutocorrectScratch::kMaxResultBytes)) {
                    scratch.frames[++length] = {child.first_child, child.child_count, frame.depth};
                }
                if (status != Utf8Status::Done) continue;
            }

            const int new_depth = depth + 1;
            const int current = new_depth * row_width;
            int first = std::max(0, new_depth - deletions);
            const int last = std::min(observed_count, new_depth + insertions);
            int cell = current + diagonal + first - new_depth;
            double minimum = infinity;
            if (first == 0) {
                minimum = scratch.rows[cell - row_width + 1] + kDeletion;
                scratch.rows[cell++] = minimum;
                first = 1;
            }
            for (int i = first; i <= last; ++i, ++cell) {
                const int prior = cell - row_width;
                const auto observed = scratch.observed[i - 1];
                double cost = std::min(scratch.rows[prior] + Substitution(scalar, observed),
                    std::min(scratch.rows[prior + 1] + kDeletion, scratch.rows[cell - 1] + kInsertion));
                if (depth > 0 && i > 1 && scratch.last_rune[depth] == observed && scalar == scratch.observed[i - 2])
                    cost = std::min(cost, scratch.rows[prior - row_width] + kSwap);
                scratch.rows[cell] = cost;
                minimum = std::min(minimum, cost);
            }
            if (last == observed_count && scratch.rows[cell - 1] <= budget && child.terminal != 0)
                Consider({scratch.path, static_cast<std::size_t>(bytes)}, child.terminal, scratch.rows[cell - 1]);

            // A transposition can bypass the latest row, so retain the prior bound too.
            if (child.child_count != 0 && new_depth < longest && (minimum <= budget || scratch.swap_can_cross[depth])) {
                scratch.rune_end[new_depth] = static_cast<std::uint16_t>(bytes);
                scratch.last_rune[new_depth] = scalar;
                scratch.swap_can_cross[new_depth] = minimum + kSwap <= budget;
                scratch.frames[++length] = {child.first_child, child.child_count, static_cast<std::uint8_t>(new_depth)};
            }
        }

        if (best_length == 0) return false;
        result = {{scratch.best_word, best_length}, 1 / total_weight};
        return true;
    }

    bool RunUtf8(std::string_view input, AutocorrectResult& result, std::string_view previous, std::string_view older) noexcept {
        result = {};
        std::string_view folded_input, folded_previous, folded_older;
        if (!FoldUtf8(input, scratch.typed, sizeof(scratch.typed), folded_input) ||
            !FoldUtf8(previous, scratch.previous, sizeof(scratch.previous), folded_previous) ||
            !FoldUtf8(older, scratch.older, sizeof(scratch.older), folded_older)) return false;
        return Run(folded_input, result, folded_previous, folded_older);
    }
};

bool BestAutocorrection(const Model& model, AutocorrectScratch& scratch, std::string_view folded_typed,
                        AutocorrectResult& result, std::string_view folded_previous, std::string_view folded_older) noexcept {
    return AutocorrectSearch{model, scratch, {}}.Run(folded_typed, result, folded_previous, folded_older);
}

bool BestAutocorrectionUtf8(const Model& model, AutocorrectScratch& scratch, std::string_view typed,
                            AutocorrectResult& result, std::string_view previous, std::string_view older) noexcept {
    return AutocorrectSearch{model, scratch, {}}.RunUtf8(typed, result, previous, older);
}

#endif
}  // namespace espos::keyboard::prediction
