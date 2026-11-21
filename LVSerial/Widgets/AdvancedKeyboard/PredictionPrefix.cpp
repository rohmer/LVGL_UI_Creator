// Ported from ESPOS components/ngram_trie; see PROVENANCE.md.
#include "PredictionPrefix.h"

#include <algorithm>
#include <cstring>

namespace espos::keyboard::prediction {
namespace {

constexpr std::uint8_t space = ' ';

bool Beats(std::uint32_t mass, std::string_view word, const Suggestion& other) noexcept
{
    return mass > other.mass || (mass == other.mass && word < other.Word());
}

std::uint32_t AfterWord(const Model& model, std::uint32_t node, std::string_view word) noexcept
{
    if (node == Model::kNotFound) return node;
    node = model.FollowFrom(node, word);
    return node == Model::kNotFound ? node : model.FindChild(node, space);
}

std::size_t Collect(const Model& model, PrefixScratch& scratch, std::uint32_t root,
                    std::string_view prefix, Suggestion* output, std::size_t seen,
                    std::size_t limit) noexcept
{
    if (root == Model::kNotFound || seen == limit) return seen;
    std::uint32_t index = model.FollowFrom(root, prefix);
    Node node;
    if (index == Model::kNotFound || !model.GetNode(index, node)) return seen;

    if (!prefix.empty()) std::memcpy(scratch.path.data(), prefix.data(), prefix.size());
    std::size_t depth = prefix.size();
    std::size_t count = 0;
    const std::size_t capacity = limit - seen;
    for (;;) {
        PrefixScratch::Frame& frame = scratch.frames[depth];
        frame = {};
        const std::string_view word{scratch.path.data(), depth};
        if (count < capacity || Beats(node.mass, word, scratch.ranked[count - 1])) {
            std::uint32_t mass = node.terminal;
            const auto space_id = model.FindChild(index, space);
            Node after_word;
            if (space_id != Model::kNotFound && model.GetNode(space_id, after_word)) mass += after_word.mass;

            const auto* already_emitted = std::find_if(output, output + seen,
                [word](const Suggestion& item) { return word == item.Word(); });
            if (depth != 0 && mass > 0 && already_emitted == output + seen) {
                std::size_t position = 0;
                while (position < count && !Beats(mass, word, scratch.ranked[position])) ++position;
                if (position < capacity) {
                    count = std::min(count + 1, capacity);
                    for (std::size_t i = count - 1; i > position; --i) scratch.ranked[i] = scratch.ranked[i - 1];
                    auto& candidate = scratch.ranked[position];
                    std::memcpy(candidate.text.data(), word.data(), depth);
                    candidate.mass = mass;
                    candidate.length = static_cast<std::uint16_t>(depth);
                }
            }

            if (depth < kSuggestionBytes) frame = {node.first_child, node.first_child + node.child_count};
        }

        for (;;) {
            auto& pending = scratch.frames[depth];
            if (pending.next_child == pending.end) {
                if (depth == prefix.size()) {
                    std::copy_n(scratch.ranked.begin(), count, output + seen);
                    return seen + count;
                }
                --depth;
                continue;
            }
            index = pending.next_child++;
            if (!model.GetNode(index, node) || node.label == space) continue;
            scratch.path[depth++] = static_cast<char>(node.label);
            break;
        }
    }
}

} // namespace

std::size_t Suggest(const Model& model, PrefixScratch& scratch, std::string_view prefix,
                    std::string_view previous, std::string_view older, Suggestion* output, std::size_t limit) noexcept
{
    if (!model.IsOpen() || !output || limit == 0 || prefix.size() > kSuggestionBytes ||
        prefix.find(' ') != std::string_view::npos) return 0;

    limit = std::min(limit, kSuggestionCount);
    std::size_t emitted = 0;
    const auto root = static_cast<std::uint32_t>(Root::Combined);
    if (!previous.empty() && !older.empty()) {
        const auto context = AfterWord(model, AfterWord(model, root, older), previous);
        emitted = Collect(model, scratch, context, prefix, output, emitted, limit);
    }
    if (emitted < limit && !previous.empty()) {
        emitted = Collect(model, scratch, AfterWord(model, root, previous), prefix, output, emitted, limit);
    }
    return Collect(model, scratch, static_cast<std::uint32_t>(Root::Unigrams), prefix, output, emitted, limit);
}

} // namespace espos::keyboard::prediction
