#pragma once

#include <cstddef>
#include <memory>

namespace espos::keyboard::prediction {

// Immutable shared model bytes. Created only when an ESP32 PSRAM allocation
// succeeds; the normal borrowed-memory path needs no storage object.
class ModelStorage {
public:
    ModelStorage(void* bytes, size_t trie_size, size_t index_size) noexcept;
    ~ModelStorage();
    ModelStorage(const ModelStorage&) = delete;
    ModelStorage& operator=(const ModelStorage&) = delete;

    const void* Trie() const { return bytes_; }
    const void* Index() const { return static_cast<const unsigned char*>(bytes_) + trie_size_; }
    size_t TrieSize() const { return trie_size_; }
    size_t IndexSize() const { return index_size_; }

private:
    void* bytes_;
    size_t trie_size_;
    size_t index_size_;
};

// Returns shared PSRAM bytes when available, otherwise an empty pointer. Calls
// with identical immutable model/index bytes share the same allocation.
std::shared_ptr<const ModelStorage> CopyModelToPsram(
    const void* trie, size_t trie_size, const void* index, size_t index_size);

} // namespace espos::keyboard::prediction
