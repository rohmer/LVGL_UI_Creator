#include "ngram_trie/esp_runtime.h"
#include "lv_advanced_keyboard_prediction.h"
#include <esp_heap_caps.h>
#include "AssetFixtures.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "Headless runtime line %d: %s\n", __LINE__, #value); return false; } } while (false)
namespace {
struct Allocation { std::size_t size; std::uint32_t caps; };
std::map<void*, Allocation> allocations;
bool psram_available = false;
std::size_t psram_allocations = 0, mappings = 0;
std::size_t expected_model_bytes = 0, expected_index_bytes = 0;
std::vector<std::uint8_t> partition_bytes;
esp_partition_t partition{};
std::size_t LivePsram()
{
    std::size_t count = 0;
    for (const auto& entry : allocations) if (entry.second.caps & MALLOC_CAP_SPIRAM) ++count;
    return count;
}
void Write32(std::size_t offset, std::uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) partition_bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
bool PrepareBytes(const std::vector<std::uint8_t>& trie)
{
    void* index = nullptr; std::size_t index_size = 0;
    CHECK(lv_advanced_keyboard_build_trie_index(trie.data(), trie.size(), &index, &index_size));
    expected_model_bytes = trie.size(); expected_index_bytes = index_size;
    partition_bytes.assign(48 + trie.size() + index_size, 0);
    std::memcpy(partition_bytes.data(), "NGPK", 4);
    Write32(4, 1); Write32(8, static_cast<std::uint32_t>(trie.size())); Write32(12, static_cast<std::uint32_t>(index_size));
    std::memcpy(partition_bytes.data() + 48, trie.data(), trie.size());
    if (index_size) std::memcpy(partition_bytes.data() + 48 + trie.size(), index, index_size);
    lv_advanced_keyboard_free_trie_index(index);
    partition.size = partition_bytes.size();
    CHECK(allocations.empty()); return true;
}
bool Prepare(const char* path)
{
    std::ifstream stream(path, std::ios::binary);
    std::vector<std::uint8_t> trie((std::istreambuf_iterator<char>(stream)), {});
    CHECK(trie.size() > 64 * 1024); // Retain the large-model regression.
    return PrepareBytes(trie);
}
bool PlacementAndLease(bool use_psram)
{
    psram_available = use_psram; psram_allocations = 0;
    ngram_trie::FlashPredictor flash;
    CHECK(flash.Open(true) == ESP_OK && flash.Trie().IsOpen());
    CHECK(flash.ModelBytes() == expected_model_bytes && flash.IndexBytes() == expected_index_bytes && mappings == 1);
    CHECK(flash.ModelUsesPsram() == use_psram && flash.UsesPsram() == use_psram);
    CHECK(LivePsram() == (use_psram ? 2u : 0u));
    auto lease = flash.ModelOwner();
    const auto model_allocations = psram_allocations;
    auto* predictor = lv_advanced_keyboard_predictor_create_borrowed(
        flash.ModelData(), flash.ModelBytes(), flash.IndexData(), flash.IndexBytes());
    CHECK(predictor && lv_advanced_keyboard_predictor_node_count(predictor) == flash.Trie().NodeCount());
    if (use_psram) CHECK(psram_allocations == model_allocations); // Reuses runtime's model copy.
    flash.Close();
    CHECK(mappings == 1 && !flash.Trie().IsOpen());
    CHECK(LivePsram() == (use_psram ? 1u : 0u));
    lv_advanced_keyboard_suggestion_t output[3]{};
    CHECK(lv_advanced_keyboard_predictor_suggest_folded(predictor, "", 0, "", 0, "", 0, output, 3) > 0);
    lv_advanced_keyboard_predictor_destroy(predictor);
    CHECK(mappings == 1); // The lease protects original flash spans independently of PSRAM.
    lease.reset();
    CHECK(mappings == 0 && allocations.empty()); return true;
}
bool InvalidImages()
{
    ngram_trie::FlashPredictor flash;
    const auto old_size = partition.size; partition.size = 47;
    CHECK(flash.Open(true) == ESP_ERR_INVALID_SIZE && mappings == 0);
    partition.size = old_size;
    const auto saved = partition_bytes[48]; partition_bytes[48] ^= 0xff;
    CHECK(flash.Open(true) == ESP_ERR_INVALID_RESPONSE && mappings == 0 && allocations.empty());
    partition_bytes[48] = saved;
    CHECK(flash.Open(false) == ESP_OK && !flash.UsesPsram());
    flash.Close(); CHECK(mappings == 0 && allocations.empty()); return true;
}
bool EmptyPartition()
{
    ngram_trie::FlashPredictor flash;
    std::array<std::uint8_t, 48> saved{};
    std::copy_n(partition_bytes.begin(), saved.size(), saved.begin());
    std::fill_n(partition_bytes.begin(), saved.size(), std::uint8_t(0xff));
    const auto status = flash.Open(true);
    std::copy(saved.begin(), saved.end(), partition_bytes.begin());
    CHECK(status == ESP_ERR_NOT_FOUND && mappings == 0 && allocations.empty()); return true;
}
}
extern "C" void* heap_caps_malloc(std::size_t bytes, std::uint32_t caps)
{
    if ((caps & MALLOC_CAP_SPIRAM) && !psram_available) return nullptr;
    auto* pointer = std::malloc(bytes);
    if (pointer) { allocations[pointer] = {bytes, caps}; if (caps & MALLOC_CAP_SPIRAM) ++psram_allocations; }
    return pointer;
}
extern "C" void* heap_caps_realloc(void* pointer, std::size_t bytes, std::uint32_t caps)
{
    if ((caps & MALLOC_CAP_SPIRAM) && !psram_available) return nullptr;
    auto* resized = std::realloc(pointer, bytes);
    if (resized) { allocations.erase(pointer); allocations[resized] = {bytes, caps}; }
    return resized;
}
extern "C" void heap_caps_free(void* pointer) { allocations.erase(pointer); std::free(pointer); }
extern "C" std::size_t heap_caps_get_total_size(std::uint32_t) { return psram_available ? 8 * 1024 * 1024 : 0; }
extern "C" std::size_t heap_caps_get_free_size(std::uint32_t caps) { return heap_caps_get_total_size(caps); }
const esp_partition_t* esp_partition_find_first(int, int, const char* name)
{
    return !std::strcmp(name, "ngram") ? &partition : nullptr;
}
esp_err_t esp_partition_read(const esp_partition_t*, std::size_t offset, void* destination, std::size_t size)
{
    if (offset > partition_bytes.size() || size > partition_bytes.size() - offset) return ESP_ERR_INVALID_SIZE;
    std::memcpy(destination, partition_bytes.data() + offset, size); return ESP_OK;
}
esp_err_t esp_partition_mmap(const esp_partition_t*, std::size_t offset, std::size_t size, int,
    const void** mapped, esp_partition_mmap_handle_t* handle)
{
    if (offset > partition_bytes.size() || size > partition_bytes.size() - offset) return ESP_ERR_INVALID_SIZE;
    *mapped = partition_bytes.data() + offset; *handle = 1; ++mappings; return ESP_OK;
}
void esp_partition_munmap(esp_partition_mmap_handle_t) { --mappings; }
int main(int argc, char** argv)
{
    bool result = argc == 2;
    if (result && !std::strcmp(argv[1], "--formats")) {
        for (const auto& bytes : {RadixFixtures::Fixed(), RadixFixtures::Radix(false), RadixFixtures::Radix(true)})
            if (!(PrepareBytes(bytes) && PlacementAndLease(true) && PlacementAndLease(false) && InvalidImages() && EmptyPartition())) result = false;
    } else result = result && Prepare(argv[1]) && PlacementAndLease(true) && PlacementAndLease(false) && InvalidImages() && EmptyPartition();
    if (result) std::puts("Headless runtime: NGPK formats, shared PSRAM/fallback, partition lease, malformed/empty images passed");
    return result ? 0 : 1;
}
