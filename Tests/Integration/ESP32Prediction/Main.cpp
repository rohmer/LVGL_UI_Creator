#include <esp_heap_caps.h>
#include <cstdlib>
#include <map>

// Exercise the existing full prediction regression suite with ESP32 allocation
// enabled, then test memory placement and lifetime through the same public API.
#define main PredictionRegressionMain
#include "../KeyboardPrediction.cpp"
#undef main

namespace {
struct Allocation { size_t size; uint32_t caps; };
std::map<void*, Allocation> allocations;
bool psram_available = true;
size_t psram_attempts = 0;
size_t psram_successes = 0;
size_t fail_psram_attempt = 0;

void ResetAllocator(bool available = true, size_t fail_attempt = 0)
{
    Check(allocations.empty(), "ESP32 model or index allocation leaked");
    psram_available = available;
    psram_attempts = psram_successes = 0;
    fail_psram_attempt = fail_attempt;
}

size_t LivePsram()
{
    size_t count = 0;
    for (const auto& entry : allocations)
        if (entry.second.caps & MALLOC_CAP_SPIRAM) ++count;
    return count;
}

bool InPsram(const void* bytes, size_t size)
{
    for (const auto& entry : allocations) {
        if (!(entry.second.caps & MALLOC_CAP_SPIRAM) || entry.second.size < size) continue;
        const auto* data = static_cast<const uint8_t*>(entry.first);
        for (size_t offset = 0; offset <= entry.second.size - size; ++offset)
            if (!std::memcmp(data + offset, bytes, size)) return true;
    }
    return false;
}

struct ModelBytes {
    std::vector<uint8_t> trie;
    std::vector<uint8_t> index;
};

ModelBytes FixtureBytes()
{
    ResetAllocator(false);
    ModelBytes result{Fixture(), {}};
    void* index = nullptr;
    size_t size = 0;
    Check(lv_advanced_keyboard_build_trie_index(result.trie.data(), result.trie.size(), &index, &size),
          "Cannot build the allocator test fixture");
    const auto* bytes = static_cast<const uint8_t*>(index);
    result.index.assign(bytes, bytes + size);
    lv_advanced_keyboard_free_trie_index(index);
    ResetAllocator();
    return result;
}

Predictor Borrow(const ModelBytes& bytes)
{
    return Predictor(lv_advanced_keyboard_predictor_create_borrowed(
        bytes.trie.data(), bytes.trie.size(), bytes.index.data(), bytes.index.size()));
}

void CheckInference(lv_advanced_keyboard_predictor_t* predictor)
{
    std::array<lv_advanced_keyboard_suggestion_t, 3> suggestions{};
    Check(lv_advanced_keyboard_predictor_suggest_folded(predictor, "hel", 3, nullptr, 0, nullptr, 0,
          suggestions.data(), suggestions.size()) == 2 && Word(suggestions[0]) == "hello",
          "PSRAM placement changed prediction results");
    Corrections(predictor);
}

void BorrowedPlacementAndSharing(const ModelBytes& bytes)
{
    ResetAllocator();
    auto first = Borrow(bytes);
    Check(bool(first), "PSRAM-backed borrowed predictor failed");
    Check(LivePsram() == 1 && InPsram(bytes.trie.data(), bytes.trie.size()) &&
          InPsram(bytes.index.data(), bytes.index.size()), "Trie and NGIX were not copied into PSRAM");
    CheckInference(first.get());
    const auto attempts = psram_attempts;
    const ModelBytes identical = bytes;
    auto second = Borrow(identical);
    Check(bool(second), "Second shared predictor failed");
    Check(LivePsram() == 1 && psram_attempts == attempts,
          "Identical model bytes were duplicated in PSRAM");
    first.reset();
    Check(LivePsram() == 1, "Destroying one predictor released another predictor's model");
    CheckInference(second.get());
    second.reset();
    Check(LivePsram() == 0, "Last predictor did not release its PSRAM model");
    auto reloaded = Borrow(bytes);
    Check(bool(reloaded) && LivePsram() == 1 && psram_successes == 2,
          "Reload did not allocate a fresh model after the final owner was released");
    CheckInference(reloaded.get());
    reloaded.reset();
    ResetAllocator();
}

void PlacementFallback(const ModelBytes& bytes)
{
    for (const bool available : {false, true}) {
        ResetAllocator(available, available ? 1 : 0);
        auto predictor = Borrow(bytes);
        Check(bool(predictor), "Unavailable or exhausted PSRAM disabled a valid borrowed model");
        Check(LivePsram() == 0, "PSRAM failure unexpectedly retained an external allocation");
        CheckInference(predictor.get());
        predictor.reset();
        ResetAllocator();
    }
}

void BuiltinPlacement()
{
    ResetAllocator();
    const void* trie = nullptr;
    const void* index = nullptr;
    size_t trie_size = 0, index_size = 0;
    Check(lv_advanced_keyboard_get_builtin_trie(&trie, &trie_size, &index, &index_size),
          "Builtin model bytes are missing");
    Predictor first(lv_advanced_keyboard_predictor_create_builtin());
    Predictor second(lv_advanced_keyboard_predictor_create_builtin());
    Check(first && second && LivePsram() == 1 && InPsram(trie, trie_size) && InPsram(index, index_size),
          "Builtin predictors did not share a PSRAM copy");
    Corrections(first.get());
    first.reset();
    Corrections(second.get());
    second.reset();
    ResetAllocator();
}

void GeneratedIndexPlacement(const ModelBytes& bytes)
{
    // Fail each of the two large construction allocations in turn. A normal
    // RAM fallback must still produce the exact same validated sidecar.
    for (const size_t fail_attempt : {size_t{0}, size_t{1}, size_t{2}}) {
        ResetAllocator(true, fail_attempt);
        void* index = nullptr;
        size_t size = 0;
        Check(lv_advanced_keyboard_build_trie_index(bytes.trie.data(), bytes.trie.size(), &index, &size),
              "Index construction did not survive a PSRAM allocation failure");
        Check(size == bytes.index.size() && !std::memcmp(index, bytes.index.data(), size) &&
              lv_advanced_keyboard_validate_trie(bytes.trie.data(), bytes.trie.size(), index, size),
              "PSRAM index construction changed sidecar bytes");
        if (!fail_attempt) Check(InPsram(index, size), "Generated NGIX was not allocated in PSRAM");
        lv_advanced_keyboard_free_trie_index(index);
        ResetAllocator();
    }
    ResetAllocator(false);
    void* compact_index = nullptr;
    size_t compact_size = 0;
    Check(lv_advanced_keyboard_build_trie_index(bytes.trie.data(), bytes.trie.size(), &compact_index, &compact_size),
          "Normal-RAM index generation failed without PSRAM");
    const auto compact_allocation = allocations.find(compact_index);
    Check(compact_allocation != allocations.end() && compact_allocation->second.size == compact_size &&
          !(compact_allocation->second.caps & MALLOC_CAP_SPIRAM),
          "Index was not shrunk to its actual size in normal RAM when PSRAM was unavailable");
    Check(compact_size == bytes.index.size() && !std::memcmp(compact_index, bytes.index.data(), compact_size),
          "Normal-RAM index compaction changed sidecar bytes");
    lv_advanced_keyboard_free_trie_index(compact_index);
    ResetAllocator();
    for (const bool available : {true, false}) {
        ResetAllocator(available);
        Predictor predictor(lv_advanced_keyboard_predictor_create(bytes.trie.data(), bytes.trie.size()));
        Check(bool(predictor), "Owned-index predictor creation failed");
        if (available) Check(InPsram(bytes.trie.data(), bytes.trie.size()), "Raw trie was not copied to PSRAM");
        CheckInference(predictor.get());
        predictor.reset();
        ResetAllocator();
    }
}

void InvalidDataAndRepeatedLoads(const ModelBytes& bytes)
{
    ResetAllocator();
    auto malformed = bytes;
    malformed.index[0] = '?';
    Check(!Borrow(malformed), "Malformed NGIX created a predictor");
    Check(allocations.empty(), "Malformed model validation leaked PSRAM");
    void* invalid = reinterpret_cast<void*>(1);
    size_t invalid_size = 123;
    Check(!lv_advanced_keyboard_build_trie_index(bytes.trie.data(), bytes.trie.size() - 1, &invalid, &invalid_size)
          && !invalid && !invalid_size, "Truncated NGTR was accepted during PSRAM index construction");
    Check(allocations.empty(), "Failed index construction leaked PSRAM");
    for (unsigned iteration = 0; iteration < 12; ++iteration) {
        auto predictor = Borrow(bytes);
        Check(bool(predictor), "Repeated trie load failed");
        CheckInference(predictor.get());
        predictor.reset();
        Check(allocations.empty(), "Repeated trie load retained its PSRAM allocation");
    }
    ResetAllocator();
}
} // namespace

extern "C" void* heap_caps_malloc(size_t size, uint32_t caps)
{
    if (caps & MALLOC_CAP_SPIRAM) {
        ++psram_attempts;
        if (!psram_available || psram_attempts == fail_psram_attempt) return nullptr;
    }
    auto* memory = std::malloc(size);
    if (memory) {
        allocations.emplace(memory, Allocation{size, caps});
        if (caps & MALLOC_CAP_SPIRAM) ++psram_successes;
    }
    return memory;
}

extern "C" void* heap_caps_realloc(void* memory, size_t size, uint32_t caps)
{
    if (caps & MALLOC_CAP_SPIRAM) {
        ++psram_attempts;
        if (!psram_available || psram_attempts == fail_psram_attempt) return nullptr;
    }
    const auto existing = allocations.find(memory);
    auto* resized = std::realloc(memory, size);
    if (resized) {
        if (existing != allocations.end()) allocations.erase(existing);
        allocations.emplace(resized, Allocation{size, caps});
    }
    return resized;
}

extern "C" void heap_caps_free(void* memory)
{
    allocations.erase(memory);
    std::free(memory);
}

extern "C" size_t heap_caps_get_total_size(uint32_t caps)
{
    return (caps & MALLOC_CAP_SPIRAM) && !psram_available ? 0 : 8 * 1024 * 1024;
}

extern "C" size_t heap_caps_get_free_size(uint32_t caps)
{
    return heap_caps_get_total_size(caps);
}

int main()
{
    try {
        Check(PredictionRegressionMain() == 0, "Existing prediction regression suite failed on the ESP32 path");
        ResetAllocator();
        const auto bytes = FixtureBytes();
        BorrowedPlacementAndSharing(bytes);
        PlacementFallback(bytes);
        BuiltinPlacement();
        GeneratedIndexPlacement(bytes);
        InvalidDataAndRepeatedLoads(bytes);
        std::cout << "ESP32 PSRAM placement, sharing, fallback, index and lifetime tests passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
