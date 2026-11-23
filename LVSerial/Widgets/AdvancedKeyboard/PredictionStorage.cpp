#include "PredictionStorage.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#include <mutex>
#include <vector>
#endif

namespace espos::keyboard::prediction {
namespace {
void FreeModelBytes(void* bytes)
{
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    heap_caps_free(bytes);
#else
    std::free(bytes);
#endif
}
}

ModelStorage::ModelStorage(void* bytes, size_t trie_size, size_t index_size) noexcept
    : bytes_(bytes), trie_size_(trie_size), index_size_(index_size) {}
ModelStorage::~ModelStorage() { FreeModelBytes(bytes_); }

std::shared_ptr<const ModelStorage> CopyModelToPsram(
    const void* trie, size_t trie_size, const void* index, size_t index_size)
{
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    if (!trie || !index || trie_size < 16 || index_size < 16 ||
        trie_size > UINT32_MAX || index_size > UINT32_MAX || trie_size > SIZE_MAX - index_size) return {};
    static std::mutex mutex;
    static std::vector<std::weak_ptr<const ModelStorage>> models;
    const std::lock_guard<std::mutex> lock(mutex);
    for (auto iterator = models.begin(); iterator != models.end();) {
        const auto model = iterator->lock();
        if (!model) { iterator = models.erase(iterator); continue; }
        if (model->TrieSize() == trie_size && model->IndexSize() == index_size &&
            (model->Trie() == trie || !std::memcmp(model->Trie(), trie, trie_size)) &&
            (model->Index() == index || !std::memcmp(model->Index(), index, index_size))) return model;
        ++iterator;
    }
    void* bytes = heap_caps_malloc(trie_size + index_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!bytes) return {};
    std::shared_ptr<const ModelStorage> model;
    try { model = std::make_shared<ModelStorage>(bytes, trie_size, index_size); }
    catch (const std::bad_alloc&) { FreeModelBytes(bytes); return {}; }
    std::memcpy(bytes, trie, trie_size);
    std::memcpy(static_cast<unsigned char*>(bytes) + trie_size, index, index_size);
    try { models.emplace_back(model); }
    catch (const std::bad_alloc&) { /* The model remains usable without caching. */ }
    return model;
#else
    (void)trie; (void)trie_size; (void)index; (void)index_size;
    return {};
#endif
}
}
