#include "ProjectTries.h"
#include "Base64.h"
#include "RuntimeFeatures.h"
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>

#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
#include "../Widgets/AdvancedKeyboard/include/lv_advanced_keyboard_prediction.h"
#include "../Widgets/AdvancedKeyboard/PredictionStorage.h"
#endif

namespace Serialization {
namespace {
using json = nlohmann::json;
constexpr size_t kMaxAssetBytes = 16u * 1024u * 1024u;
constexpr const char* kBuiltin = "builtin-en-us";
struct Bytes {
    Binary owned;
    const uint8_t* borrowed = nullptr;
    size_t borrowedSize = 0;
    const uint8_t* Data() const { return borrowed ? borrowed : owned.data(); }
    size_t Size() const { return borrowed ? borrowedSize : owned.size(); }
};
struct Asset {
    Bytes trie, index;
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
    std::shared_ptr<const espos::keyboard::prediction::ModelStorage> storage;
#endif
};
void PreferPsram(Asset& asset)
{
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
    asset.storage = espos::keyboard::prediction::CopyModelToPsram(
        asset.trie.Data(), asset.trie.Size(), asset.index.Data(), asset.index.Size());
    if (asset.storage) {
        asset.trie.borrowed = static_cast<const uint8_t*>(asset.storage->Trie());
        asset.trie.borrowedSize = asset.storage->TrieSize();
        asset.index.borrowed = static_cast<const uint8_t*>(asset.storage->Index());
        asset.index.borrowedSize = asset.storage->IndexSize();
        Binary{}.swap(asset.trie.owned);
        Binary{}.swap(asset.index.owned);
    }
#else
    (void)asset;
#endif
}
uint32_t Read32(const uint8_t* bytes)
{
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}
void Header(const Bytes& bytes)
{
    if (bytes.Size() < 20 || bytes.Size() > kMaxAssetBytes || std::memcmp(bytes.Data(), "NGTR", 4) ||
        Read32(bytes.Data() + 4) != 3 || Read32(bytes.Data() + 12) != 0)
        throw std::invalid_argument("Trie must be a compact NGTR v3 model between 20 bytes and 16 MiB");
    const auto nodes = Read32(bytes.Data() + 8);
    if (nodes < 2 || nodes > (bytes.Size() - 16) / 2 || bytes.Size() - 16 > uint64_t(nodes) * 8)
        throw std::invalid_argument("Trie node count does not match its byte length");
}
std::string Base64(const Bytes& bytes)
{
    return base64_encode(bytes.Data(), static_cast<unsigned int>(bytes.Size()));
}
Bytes Read(const json& value)
{
    Bytes result;
    if (value.is_binary()) result.owned.assign(value.get_binary().begin(), value.get_binary().end());
    else if (value.is_string()) {
        const auto& text = value.get_ref<const std::string&>();
        if (text.size() > ((kMaxAssetBytes + 2) / 3) * 4 || text.size() % 4)
            throw std::invalid_argument("Trie asset has invalid base64 size");
        auto decoded = base64_decode(text);
        result.owned.assign(decoded.begin(), decoded.end());
        if (Base64(result) != text) throw std::invalid_argument("Trie asset must use canonical base64");
    } else throw std::invalid_argument("Trie assets require embedded base64 or binary data");
    if (!result.Size() || result.Size() > kMaxAssetBytes) throw std::invalid_argument("Trie asset is empty or exceeds 16 MiB");
    return result;
}
Bytes Take(json& value, const std::string& path, const BorrowedAssets& borrowed)
{
    if (value.is_object() && value.size() == 2 && value.contains("borrowedBytes") && value.contains("size")) {
        const auto found = borrowed.find(path);
        if (!value.at("borrowedBytes").is_string() || value.at("borrowedBytes") != path ||
            found == borrowed.end() || !found->second.data || !found->second.size ||
            !value.at("size").is_number_unsigned() || value.at("size").get<size_t>() != found->second.size ||
            found->second.size > kMaxAssetBytes)
            throw std::invalid_argument("Invalid borrowed trie asset");
        Bytes result; result.borrowed = found->second.data; result.borrowedSize = found->second.size; return result;
    }
    if (value.is_binary()) {
        Bytes result; result.owned = std::move(value.get_binary());
        if (!result.Size() || result.Size() > kMaxAssetBytes) throw std::invalid_argument("Trie asset is empty or exceeds 16 MiB");
        return result;
    }
    return Read(value);
}
void Validate(Asset& asset)
{
    Header(asset.trie);
    if (!asset.index.Size()) {
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
        void* generated = nullptr; size_t size = 0;
        if (!lv_advanced_keyboard_build_trie_index(asset.trie.Data(), asset.trie.Size(), &generated, &size))
            throw std::invalid_argument("Invalid trie structure or insufficient memory for its index");
        try { asset.index.owned.assign(static_cast<const uint8_t*>(generated), static_cast<const uint8_t*>(generated) + size); }
        catch (...) { lv_advanced_keyboard_free_trie_index(generated); throw; }
        lv_advanced_keyboard_free_trie_index(generated);
#else
        throw std::invalid_argument("Prediction support is required to generate a missing trie index");
#endif
    }
    if (asset.index.Size() < 16 || asset.index.Size() > kMaxAssetBytes ||
        std::memcmp(asset.index.Data(), "NGIX", 4) || Read32(asset.index.Data() + 4) != 1 ||
        Read32(asset.index.Data() + 8) != asset.trie.Size() ||
        Read32(asset.index.Data() + 12) != Read32(asset.trie.Data() + 8))
        throw std::invalid_argument("Trie index does not match the embedded NGTR model");
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
    if (!lv_advanced_keyboard_validate_trie(asset.trie.Data(), asset.trie.Size(), asset.index.Data(), asset.index.Size()))
        throw std::invalid_argument("Trie/index failed complete structure validation");
#endif
}
std::string Identity(const Bytes& bytes)
{
    // Hash narrows candidate IDs; byte comparison below decides equality.
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < bytes.Size(); ++index) { hash ^= bytes.Data()[index]; hash *= UINT64_C(1099511628211); }
    std::string identity = "trie-0000000000000000";
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned index = 0; index < 16; ++index)
        identity[5 + index] = digits[(hash >> ((15 - index) * 4)) & 15];
    return identity;
}
bool Equal(const Bytes& first, const Bytes& second)
{
    return first.Size() == second.Size() && (!first.Size() || !std::memcmp(first.Data(), second.Data(), first.Size()));
}
void Metadata(const json& item, std::set<std::string>& ids)
{
    if (!item.is_object() || !item.contains("id") || !item.at("id").is_string() ||
        !item.contains("name") || !item.at("name").is_string() || item.value("format", std::string("ngtr")) != "ngtr")
        throw std::invalid_argument("Invalid project trie record");
    const auto& id = item.at("id").get_ref<const std::string&>();
    const auto& name = item.at("name").get_ref<const std::string&>();
    if (id.empty() || id.size() > 128 || id == kBuiltin || id.find_first_of("\r\n") != std::string::npos ||
        id.find('\0') != std::string::npos || name.empty() || name.size() > 256 ||
        name.find_first_of("\r\n") != std::string::npos || name.find('\0') != std::string::npos || !ids.insert(id).second)
        throw std::invalid_argument("Trie IDs must be unique, nonempty and distinct from the reserved built-in model");
}
void Array(const json& tries)
{
    if (!tries.is_array() || tries.size() > 256) throw std::invalid_argument("Project tries must be an array of at most 256 models");
}
}

struct ProjectTries::Catalog { std::map<std::string, std::shared_ptr<Asset>> assets; };
namespace {
thread_local ProjectTries::CatalogPtr currentCatalog;
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
struct Attachment {
    std::string id;
    std::shared_ptr<Asset> asset;
    lv_advanced_keyboard_predictor_t* predictor = nullptr;
    ~Attachment() { lv_advanced_keyboard_predictor_destroy(predictor); }
};
void DeleteAttachment(lv_event_t* event) { delete static_cast<Attachment*>(lv_event_get_user_data(event)); }
Attachment* Attached(lv_obj_t* widget)
{
    for (uint32_t index = 0; index < lv_obj_get_event_count(widget); ++index) {
        auto* descriptor = lv_obj_get_event_dsc(widget, index);
        if (lv_event_dsc_get_cb(descriptor) == DeleteAttachment)
            return static_cast<Attachment*>(lv_event_dsc_get_user_data(descriptor));
    }
    return nullptr;
}
#endif
}

ProjectTries::Scope::Scope(const CatalogPtr& catalog) : previous_(currentCatalog) { currentCatalog = catalog; }
ProjectTries::Scope::~Scope() { currentCatalog = std::move(previous_); }

nlohmann::json ProjectTries::Normalize(const json& tries)
{
    Array(tries); std::set<std::string> ids; auto result = json::array();
    for (const auto& item : tries) {
        Metadata(item, ids); Asset asset; asset.trie = Read(item.at("data"));
        if (item.contains("indexData")) asset.index = Read(item.at("indexData"));
        Validate(asset);
        result.push_back({{"id", item.at("id")}, {"name", item.at("name")}, {"format", "ngtr"},
                          {"data", Base64(asset.trie)}, {"indexData", Base64(asset.index)}});
    }
    return result;
}
nlohmann::json ProjectTries::Import(const json& tries, const std::string& name, const void* bytes, size_t size, std::string* selectedId)
{
    if (!bytes || !size || size > kMaxAssetBytes) throw std::invalid_argument("Trie file is empty or exceeds 16 MiB");
    auto result = Normalize(tries); Asset imported;
    imported.trie.borrowed = static_cast<const uint8_t*>(bytes); imported.trie.borrowedSize = size;
    Validate(imported);
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
    const void* builtin = nullptr; const void* index = nullptr; size_t builtinSize = 0, indexSize = 0;
    if (lv_advanced_keyboard_get_builtin_trie(&builtin, &builtinSize, &index, &indexSize) &&
        size == builtinSize && !std::memcmp(bytes, builtin, size)) {
        if (selectedId) *selectedId = kBuiltin;
        return result;
    }
#endif
    for (const auto& item : result) {
        const auto existing = Read(item.at("data"));
        if (Equal(existing, imported.trie)) { if (selectedId) *selectedId = item.at("id").get<std::string>(); return result; }
    }
    auto id = Identity(imported.trie); auto stem = id; size_t suffix = 0;
    for (;;) {
        bool exists = false; for (const auto& item : result) if (item.at("id") == id) { exists = true; break; }
        if (!exists) break;
        id = stem + "-" + std::to_string(++suffix);
    }
    const auto slash = name.find_last_of("/\\"); const auto display = slash == std::string::npos ? name : name.substr(slash + 1);
    result.push_back({{"id", id}, {"name", display}, {"format", "ngtr"},
                      {"data", Base64(imported.trie)}, {"indexData", Base64(imported.index)}});
    result = Normalize(result);
    if (selectedId) *selectedId = id;
    return result;
}
nlohmann::json ProjectTries::ToWire(const json& tries)
{
    auto result = Normalize(tries);
    for (auto& item : result) for (const auto* key : {"data", "indexData"}) {
        auto bytes = Read(item.at(key)); item[key] = json::binary(std::move(bytes.owned));
    }
    return result;
}
ProjectTries::CatalogPtr ProjectTries::Load(json& document, const BorrowedAssets& borrowed)
{
    auto catalog = std::make_shared<Catalog>();
    if (!document.contains("tries")) return catalog;
    auto tries = std::move(document.at("tries")); document.erase("tries");
    Array(tries); std::set<std::string> ids; size_t position = 0;
    std::map<std::string, std::vector<std::shared_ptr<Asset>>> byContent;
    for (auto& item : tries) {
        Metadata(item, ids); auto asset = std::make_shared<Asset>();
        const auto path = "/tries/" + std::to_string(position++);
        asset->trie = Take(item.at("data"), path + "/data", borrowed);
        if (item.contains("indexData")) asset->index = Take(item.at("indexData"), path + "/indexData", borrowed);
        Validate(*asset);
        auto& matches = byContent[Identity(asset->trie)];
        bool shared = false;
        for (const auto& existing : matches) if (Equal(existing->trie, asset->trie) && Equal(existing->index, asset->index)) { asset = existing; shared = true; break; }
        if (!shared) PreferPsram(*asset);
        matches.push_back(asset); catalog->assets.emplace(item.at("id").get<std::string>(), std::move(asset));
    }
    return catalog;
}
void ProjectTries::Attach(lv_obj_t* widget, const std::string& trieId, bool autocorrect, double threshold)
{
    if (!widget || !std::isfinite(threshold) || threshold < 0 || threshold > 1)
        throw std::invalid_argument("Invalid Advanced Keyboard predictor attachment");
#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
    if (!lv_advanced_keyboard_is_instance(widget)) throw std::invalid_argument("Predictor target is not an Advanced Keyboard");
    auto* attached = Attached(widget);
    std::shared_ptr<Asset> asset;
    if (!trieId.empty() && trieId != kBuiltin) {
        if (currentCatalog) { const auto found = currentCatalog->assets.find(trieId); if (found != currentCatalog->assets.end()) asset = found->second; }
        if (!asset && attached && attached->id == trieId) asset = attached->asset;
        if (!asset) throw std::invalid_argument("Unknown project trie: " + trieId);
    }
    if (attached && attached->id == trieId && attached->asset == asset &&
        lv_advanced_keyboard_get_predictor(widget) == attached->predictor) {
        lv_advanced_keyboard_set_autocorrect(widget, autocorrect);
        lv_advanced_keyboard_set_autocorrect_threshold(widget, threshold); return;
    }
    auto* predictor = trieId.empty() ? nullptr : asset ? lv_advanced_keyboard_predictor_create_borrowed(
        asset->trie.Data(), asset->trie.Size(), asset->index.Data(), asset->index.Size()) : lv_advanced_keyboard_predictor_create_builtin();
    if (!trieId.empty() && !predictor) throw std::invalid_argument("Cannot create predictor; model support may be disabled or memory exhausted");
    if (!attached) {
        auto next = std::make_unique<Attachment>();
        if (!lv_obj_add_event_cb(widget, DeleteAttachment, LV_EVENT_DELETE, next.get())) {
            lv_advanced_keyboard_predictor_destroy(predictor); throw std::bad_alloc();
        }
        attached = next.release();
    }
    lv_advanced_keyboard_set_predictor(widget, predictor);
    lv_advanced_keyboard_predictor_destroy(attached->predictor);
    attached->predictor = predictor; attached->asset = std::move(asset); attached->id = trieId;
    lv_advanced_keyboard_set_autocorrect(widget, autocorrect);
    lv_advanced_keyboard_set_autocorrect_threshold(widget, threshold);
#else
    (void)autocorrect;
    if (!trieId.empty()) throw std::invalid_argument("Prediction is disabled in this firmware profile");
#endif
}
}
