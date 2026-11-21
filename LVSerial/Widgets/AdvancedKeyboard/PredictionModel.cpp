// Ported from ESPOS components/ngram_trie; see PROVENANCE.md.
#include "PredictionModel.h"

#include <cstring>

namespace espos::keyboard::prediction {
namespace {

// NGIX v1 uses a little-endian header: "NGIX", version, NGTR byte length,
// and node count. Each 32-node block has one checkpoint holding the NGTR
// record offset, next BFS child index, and range-relative mass offset at
// the start of the block. Its mass offset points into the stream after all
// checkpoints, where only nonleaf masses appear, in BFS order as ULEB128.
constexpr uint32_t kTrieHeaderSize = 16;
constexpr uint32_t kIndexHeaderSize = 16;
constexpr uint32_t kCheckpointSize = 12;
constexpr uint32_t kNodesPerCheckpoint = 32;

uint32_t ReadLe32(const uint8_t* data)
{
    return uint32_t{data[0]} | (uint32_t{data[1]} << 8) |
           (uint32_t{data[2]} << 16) | (uint32_t{data[3]} << 24);
}

bool ReadVarUInt32(const uint8_t* bytes, uint32_t size, uint32_t& offset, uint32_t& value)
{
    value = 0;
    for (unsigned shift = 0; shift <= 28 && offset < size; shift += 7) {
        const uint8_t next = bytes[offset++];
        if (shift == 28 && next > 0x0f) {
            return false;
        }
        value |= (uint32_t{next} & 0x7f) << shift;
        if ((next & 0x80) == 0) {
            return shift == 0 || next != 0;
        }
    }
    return false;
}

}  // namespace

bool Model::Open(const void* model, size_t model_size, const void* index, size_t index_size)
{
    Close();
    if (model == nullptr || index == nullptr || model_size < kTrieHeaderSize ||
        index_size < kIndexHeaderSize || model_size > UINT32_MAX || index_size > UINT32_MAX) {
        return false;
    }

    const auto* trie = static_cast<const uint8_t*>(model);
    const auto* sidecar = static_cast<const uint8_t*>(index);
    if (std::memcmp(trie, "NGTR", 4) != 0 || ReadLe32(trie + 4) != 3 ||
        ReadLe32(trie + 12) != 0 || std::memcmp(sidecar, "NGIX", 4) != 0 ||
        ReadLe32(sidecar + 4) != 1 || ReadLe32(sidecar + 8) != model_size) {
        return false;
    }

    const uint32_t count = ReadLe32(trie + 8);
    const size_t payload_size = model_size - kTrieHeaderSize;
    if (count < 2 || count > payload_size / 2 || payload_size > uint64_t{count} * 8 ||
        ReadLe32(sidecar + 12) != count) {
        return false;
    }

    const size_t checkpoint_count = (count + kNodesPerCheckpoint - 1) / kNodesPerCheckpoint;
    const size_t mass_start = kIndexHeaderSize + checkpoint_count * kCheckpointSize;
    if (index_size < mass_start) {
        return false;
    }

    records_ = trie;
    checkpoints_ = sidecar + kIndexHeaderSize;
    masses_ = sidecar + mass_start;
    records_size_ = static_cast<uint32_t>(model_size);
    masses_size_ = static_cast<uint32_t>(index_size - mass_start);
    node_count_ = count;
    if (Validate()) {
        return true;
    }
    Close();
    return false;
}

void Model::Close()
{
    *this = Model{};
}

bool Model::Cursor::Next(Node& out)
{
    return model_ != nullptr && model_->Decode(*this, out);
}

Model::Cursor Model::AtCheckpoint(uint32_t node_index, bool include_mass) const
{
    const uint32_t block = node_index / kNodesPerCheckpoint;
    const uint8_t* checkpoint = checkpoints_ + block * kCheckpointSize;
    Cursor cursor;
    cursor.model_ = this;
    cursor.node_index_ = block * kNodesPerCheckpoint;
    cursor.record_offset_ = ReadLe32(checkpoint);
    cursor.next_child_ = ReadLe32(checkpoint + 4);
    cursor.mass_offset_ = ReadLe32(checkpoint + 8);
    cursor.include_mass_ = include_mass;
    return cursor;
}

Model::Cursor Model::ReadFrom(uint32_t node_index, bool include_mass) const
{
    if (node_index >= node_count_) {
        return {};
    }
    Cursor cursor = AtCheckpoint(node_index, include_mass);
    Node skipped;
    while (cursor.node_index_ < node_index) {
        if (!cursor.Next(skipped)) {
            return {};
        }
    }
    return cursor;
}

bool Model::GetNode(uint32_t node_index, Node& out, bool include_mass) const
{
    return ReadFrom(node_index, include_mass).Next(out);
}

uint32_t Model::FindChild(uint32_t node_index, uint8_t label) const
{
    Node parent;
    return GetNode(node_index, parent, false) ? FindChildAndRead(parent, label) : kNotFound;
}

uint32_t Model::FindChildAndRead(Node& current, uint8_t label) const
{
    if (label == 0 || current.child_count == 0) {
        return kNotFound;
    }

    Cursor cursor = ReadFrom(current.first_child, false);
    for (uint32_t offset = 0; offset < current.child_count; ++offset) {
        Node child;
        if (!cursor.Next(child) || child.label > label) {
            break;
        }
        if (child.label == label) {
            const uint32_t index = current.first_child + offset;
            current = child;
            return index;
        }
    }
    return kNotFound;
}

uint32_t Model::FollowFrom(uint32_t node_index, std::string_view suffix) const
{
    if (suffix.empty()) {
        return node_index < node_count_ ? node_index : kNotFound;
    }

    Node child;
    if (!GetNode(node_index, child, false)) {
        return kNotFound;
    }

    for (unsigned char label : suffix) {
        node_index = FindChildAndRead(child, label);
        if (node_index == kNotFound) {
            break;
        }
    }
    return node_index;
}

uint32_t Model::FindPrefix(std::string_view prefix, Root root) const
{
    if (root != Root::Combined && root != Root::Unigrams) {
        return kNotFound;
    }
    return FollowFrom(static_cast<uint32_t>(root), prefix);
}

bool Model::Decode(Cursor& cursor, Node& out) const
{
    if (cursor.node_index_ >= node_count_ || cursor.node_index_ >= cursor.next_child_ ||
        cursor.next_child_ > node_count_ || cursor.record_offset_ >= records_size_) {
        return false;
    }

    uint32_t flags;
    const uint8_t label = records_[cursor.record_offset_++];
    if (!ReadVarUInt32(records_, records_size_, cursor.record_offset_, flags) ||
        (cursor.node_index_ < 2 ? label != 0 : label == 0)) {
        return false;
    }

    const uint32_t count = flags >> 1;
    if (count > UINT8_MAX || count > node_count_ - cursor.next_child_) {
        return false;
    }

    uint32_t terminal = 0;
    if ((flags & 1) != 0 &&
        (cursor.node_index_ < 2 ||
         !ReadVarUInt32(records_, records_size_, cursor.record_offset_, terminal) || terminal == 0)) {
        return false;
    }
    if (cursor.node_index_ >= 2 && count == 0 && terminal == 0) {
        return false;
    }

    uint32_t mass = 0;
    if (cursor.include_mass_) {
        mass = terminal;
        if (count != 0 && (!ReadVarUInt32(masses_, masses_size_, cursor.mass_offset_, mass) || mass == 0)) {
            return false;
        }
    }

    out = {mass, terminal, count == 0 ? 0 : cursor.next_child_, static_cast<uint16_t>(count), label};
    ++cursor.node_index_;
    cursor.next_child_ += count;
    return true;
}

bool Model::Validate() const
{
    Cursor parents;
    parents.model_ = this;
    parents.record_offset_ = kTrieHeaderSize;
    parents.next_child_ = 2;
    Cursor children = parents;
    Node ignored;
    if (!children.Next(ignored) || !children.Next(ignored)) {
        return false;
    }

    for (uint32_t i = 0; i < node_count_; ++i) {
        if (i % kNodesPerCheckpoint == 0) {
            const Cursor checkpoint = AtCheckpoint(i, true);
            if (checkpoint.record_offset_ != parents.record_offset_ ||
                checkpoint.next_child_ != parents.next_child_ || checkpoint.mass_offset_ != parents.mass_offset_) {
                return false;
            }
        }
        Node parent;
        if (!parents.Next(parent) || parent.terminal > parent.mass) {
            return false;
        }

        // Child runs are consecutive in BFS order, so both cursors advance only forwards.
        uint32_t remaining_mass = parent.mass - parent.terminal;
        uint8_t previous_label = 0;
        for (uint32_t child_index = 0; child_index < parent.child_count; ++child_index) {
            Node child;
            if (!children.Next(child) || child.label <= previous_label || child.mass > remaining_mass) {
                return false;
            }
            remaining_mass -= child.mass;
            previous_label = child.label;
        }
        if (remaining_mass != 0) {
            return false;
        }
    }

    return parents.next_child_ == node_count_ && children.node_index_ == node_count_ &&
           parents.record_offset_ == records_size_ && parents.mass_offset_ == masses_size_;
}

}  // namespace espos::keyboard::prediction
