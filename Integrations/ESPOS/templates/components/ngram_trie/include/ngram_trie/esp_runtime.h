#pragma once
#include "ngram_trie/autocorrect.h"
#include "ngram_trie/prefix.h"
#include "esp_err.h"
#include "esp_partition.h"
#include <memory>

namespace ngram_trie {
// Reusable single-caller inference workspace, shared immutable model storage.
class FlashPredictor {
public:
    FlashPredictor() = default;
    ~FlashPredictor() { Close(); }
    FlashPredictor(const FlashPredictor&) = delete;
    FlashPredictor& operator=(const FlashPredictor&) = delete;
    esp_err_t Open(bool prefer_psram);
    void Close() noexcept;
    // Compatibility: UsesPsram reports the scratch location. Model PSRAM is
    // preferred independently whenever the hardware has available PSRAM.
    bool UsesPsram() const noexcept { return uses_psram_; }
    bool ModelUsesPsram() const noexcept;
    static std::size_t ScratchBytes() noexcept;
    std::size_t MappedBytes() const noexcept;
    const std::uint8_t* ModelHash() const noexcept;
    const Model& Trie() const noexcept { return model_; }
    // Original immutable partition spans; the canonical shared cache reuses
    // their PSRAM copy. Keep ModelOwner() alive while borrowing these bytes.
    const void* ModelData() const noexcept;
    std::size_t ModelBytes() const noexcept;
    const void* IndexData() const noexcept;
    std::size_t IndexBytes() const noexcept;
    std::shared_ptr<const void> ModelOwner() const noexcept;
    std::size_t SuggestFolded(std::string_view prefix, std::string_view previous, std::string_view older,
        Suggestion* output, std::size_t limit = kSuggestionCount) noexcept;
    bool Autocorrect(std::string_view typed, AutocorrectResult& result,
        std::string_view previous = {}, std::string_view older = {}, double threshold = 0.3) noexcept;
private:
    struct Workspace;
    struct Image;
    Model model_;
    Workspace* workspace_ = nullptr;
    std::shared_ptr<Image> image_;
    bool uses_psram_ = false;
};
FlashPredictor& DefaultPredictor();
void StartTrieRuntime();
}
