// Ported from ESPOS components/ngram_trie; see PROVENANCE.md.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace espos::keyboard::prediction {

enum class Root : uint8_t { Combined = 0, Unigrams = 1 };

struct Node {
    uint32_t mass;
    uint32_t terminal;
    uint32_t first_child;
    uint16_t child_count;
    uint8_t label;
};

// Borrows immutable NGTR v3 and NGIX v1 bytes. Both ranges must remain readable
// until Close or destruction. The model owns no heap storage.
class Model {
public:
    static constexpr uint32_t kNotFound = UINT32_MAX;

    class Cursor {
    public:
        // Read the next node in BFS order. A failed read leaves out unchanged.
        bool Next(Node& out);

    private:
        friend class Model;
        const Model* model_ = nullptr;
        uint32_t node_index_ = 0;
        uint32_t record_offset_ = 0;
        uint32_t next_child_ = 0;
        uint32_t mass_offset_ = 0;
        bool include_mass_ = true;
    };

    // Validates both ranges in place. On failure the model is closed.
    bool Open(const void* model, size_t model_size, const void* index, size_t index_size);
    void Close();
    bool IsOpen() const { return records_ != nullptr; }
    uint32_t NodeCount() const { return node_count_; }

    // Set include_mass to false for traversal using only exact terminal counts.
    // The returned mass is then zero. Close or Open invalidates existing cursors.
    Cursor ReadFrom(uint32_t node_index, bool include_mass = true) const;
    bool GetNode(uint32_t node_index, Node& out, bool include_mass = true) const;
    uint32_t FindChild(uint32_t node_index, uint8_t label) const;
    uint32_t FollowFrom(uint32_t node_index, std::string_view suffix) const;
    uint32_t FindPrefix(std::string_view prefix, Root root = Root::Combined) const;

private:
    uint32_t FindChildAndRead(Node& current, uint8_t label) const;
    Cursor AtCheckpoint(uint32_t node_index, bool include_mass) const;
    bool Decode(Cursor& cursor, Node& out) const;
    bool Validate() const;

    const uint8_t* records_ = nullptr;
    const uint8_t* checkpoints_ = nullptr;
    const uint8_t* masses_ = nullptr;
    uint32_t records_size_ = 0;
    uint32_t masses_size_ = 0;
    uint32_t node_count_ = 0;
};

}  // namespace espos::keyboard::prediction
