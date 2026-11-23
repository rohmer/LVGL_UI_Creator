#include "ngram_trie/esp_runtime.h"
#include "PredictionStorage.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <array>
#include <cstring>
#include <new>

namespace ngram_trie {
namespace {
constexpr char tag[] = "ngram_trie";
constexpr std::size_t header_bytes = 48;
constexpr std::size_t model_byte_limit = 16 * 1024 * 1024;
std::uint32_t ReadUInt32(const std::uint8_t* data) noexcept
{
    return std::uint32_t(data[0]) | (std::uint32_t(data[1]) << 8) |
        (std::uint32_t(data[2]) << 16) | (std::uint32_t(data[3]) << 24);
}
}
struct FlashPredictor::Workspace {
    AutocorrectScratch autocorrect;
    PrefixScratch prefix;
};
struct FlashPredictor::Image {
    esp_partition_mmap_handle_t mapping = 0;
    std::size_t mapped_bytes = 0;
    const std::uint8_t* model = nullptr;
    const std::uint8_t* index = nullptr;
    std::size_t model_bytes = 0, index_bytes = 0;
    std::array<std::uint8_t, 32> hash{};
    std::shared_ptr<const espos::keyboard::prediction::ModelStorage> storage;
    ~Image() { if (mapped_bytes) esp_partition_munmap(mapping); }
};
std::size_t FlashPredictor::ScratchBytes() noexcept { return sizeof(Workspace); }
bool FlashPredictor::ModelUsesPsram() const noexcept { return image_ && bool(image_->storage); }
std::size_t FlashPredictor::MappedBytes() const noexcept { return image_ ? image_->mapped_bytes : 0; }
const std::uint8_t* FlashPredictor::ModelHash() const noexcept { return image_ ? image_->hash.data() : nullptr; }
const void* FlashPredictor::ModelData() const noexcept { return image_ ? image_->model : nullptr; }
std::size_t FlashPredictor::ModelBytes() const noexcept { return image_ ? image_->model_bytes : 0; }
const void* FlashPredictor::IndexData() const noexcept { return image_ ? image_->index : nullptr; }
std::size_t FlashPredictor::IndexBytes() const noexcept { return image_ ? image_->index_bytes : 0; }
std::shared_ptr<const void> FlashPredictor::ModelOwner() const noexcept { return image_; }

esp_err_t FlashPredictor::Open(bool prefer_psram)
{
    Close();
    const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "ngram");
    if (!partition) return ESP_ERR_NOT_FOUND;
    if (partition->size < header_bytes) return ESP_ERR_INVALID_SIZE;
    std::uint8_t header[header_bytes];
    auto status = esp_partition_read(partition, 0, header, sizeof(header));
    if (status != ESP_OK) return status;
    if (std::memcmp(header, "NGPK", 4) || ReadUInt32(header + 4) != 1) return ESP_ERR_INVALID_RESPONSE;
    const auto model_size = ReadUInt32(header + 8), index_size = ReadUInt32(header + 12);
    if (model_size > model_byte_limit || model_size > partition->size - header_bytes ||
        index_size > partition->size - header_bytes - model_size) return ESP_ERR_INVALID_SIZE;
    std::shared_ptr<Image> image;
    try { image = std::make_shared<Image>(); }
    catch (const std::bad_alloc&) { return ESP_ERR_NO_MEM; }
    const auto bytes = header_bytes + model_size + index_size;
    const void* mapped = nullptr;
    status = esp_partition_mmap(partition, 0, bytes, ESP_PARTITION_MMAP_DATA, &mapped, &image->mapping);
    if (status != ESP_OK) return status;
    image->mapped_bytes = bytes;
    image->model = static_cast<const std::uint8_t*>(mapped) + header_bytes;
    image->index = image->model + model_size;
    image->model_bytes = model_size; image->index_bytes = index_size;
    std::memcpy(image->hash.data(), header + 16, image->hash.size());
    // Validate before copying untrusted partition data or allocating workspace.
    if (!model_.Open(image->model, model_size, image->index, index_size)) return ESP_ERR_INVALID_RESPONSE;
    image->storage = espos::keyboard::prediction::CopyModelToPsram(image->model, model_size, image->index, index_size);
    if (image->storage && !model_.Open(image->storage->Trie(), model_size, image->storage->Index(), index_size)) {
        model_.Close(); return ESP_ERR_INVALID_RESPONSE;
    }
    image_ = std::move(image);
    void* scratch = nullptr;
    if (prefer_psram) {
        scratch = heap_caps_malloc(ScratchBytes(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        uses_psram_ = scratch != nullptr;
        if (!scratch) ESP_LOGW(tag, "PSRAM scratch unavailable, trying internal RAM");
    }
    if (!scratch) scratch = heap_caps_malloc(ScratchBytes(), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!scratch) { Close(); return ESP_ERR_NO_MEM; }
    workspace_ = new (scratch) Workspace;
    return ESP_OK;
}
void FlashPredictor::Close() noexcept
{
    if (workspace_) { workspace_->~Workspace(); heap_caps_free(workspace_); workspace_ = nullptr; }
    model_.Close(); image_.reset(); uses_psram_ = false;
}
std::size_t FlashPredictor::SuggestFolded(std::string_view prefix, std::string_view previous,
    std::string_view older, Suggestion* output, std::size_t limit) noexcept
{
    return workspace_ ? Suggest(model_, workspace_->prefix, prefix, previous, older, output, limit) : 0;
}
bool FlashPredictor::Autocorrect(std::string_view typed, AutocorrectResult& result,
    std::string_view previous, std::string_view older, double threshold) noexcept
{
    result = {};
    if (!workspace_ || !(threshold >= 0 && threshold <= 1)) return false;
    if (BestAutocorrectionUtf8(model_, workspace_->autocorrect, typed, result, previous, older) &&
        result.probability >= threshold) return true;
    result = {}; return false;
}
FlashPredictor& DefaultPredictor() { static FlashPredictor predictor; return predictor; }
#ifdef ESPOS_NGRAM_BENCHMARK
void StartTrieBenchmark(bool prefer_psram);
#endif
void StartTrieRuntime()
{
    constexpr bool prefer_psram =
#ifdef CONFIG_ESPOS_NGRAM_SCRATCH_PSRAM
        true;
#else
        false;
#endif
#ifdef ESPOS_NGRAM_BENCHMARK
    StartTrieBenchmark(prefer_psram);
#else
    auto& trie = DefaultPredictor();
    const auto status = trie.Open(prefer_psram);
    if (status == ESP_OK) {
        ESP_LOGI(tag, "trie %zu + index %zu bytes in %s, %lu nodes, scratch %zu bytes in %s",
            trie.ModelBytes(), trie.IndexBytes(), trie.ModelUsesPsram() ? "shared PSRAM" : "mapped flash",
            static_cast<unsigned long>(trie.Trie().NodeCount()), trie.ScratchBytes(),
            trie.UsesPsram() ? "PSRAM" : "internal RAM");
    } else if (status != ESP_ERR_NOT_FOUND) ESP_LOGW(tag, "could not start trie: %s", esp_err_to_name(status));
#endif
}
}
