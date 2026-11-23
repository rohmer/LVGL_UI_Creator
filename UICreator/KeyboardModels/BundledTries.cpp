#include "BundledTries.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace BundledTries {
namespace {
struct Entry {
    const char *id, *language, *name, *sourceName;
    uint32_t sizeKiB;
    size_t bytes;
    const char *filename, *projectTrieId;
};
constexpr Entry entries[] = {
#include "CatalogData.inc"
};
std::filesystem::path Resources()
{
    const auto* executable = SDL_GetBasePath();
    if (executable) {
        auto installed = std::filesystem::u8path(executable) / "language-models" / "bundled";
        if (std::filesystem::is_directory(installed)) return installed;
    }
    // Development and isolated tests can use the checked-in resource copy.
    auto source = std::filesystem::path(__FILE__).parent_path() / "models";
    if (std::filesystem::is_directory(source)) return source;
    return executable ? std::filesystem::u8path(executable) / "language-models" / "bundled" : source;
}
uint32_t Read32(const uint8_t* value)
{
    return uint32_t(value[0]) | (uint32_t(value[1]) << 8) |
        (uint32_t(value[2]) << 16) | (uint32_t(value[3]) << 24);
}
std::string Identity(const std::vector<uint8_t>& bytes)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const auto value : bytes) { hash ^= value; hash *= UINT64_C(1099511628211); }
    constexpr char digits[] = "0123456789abcdef";
    std::string identity = "trie-0000000000000000";
    for (unsigned index = 0; index < 16; ++index)
        identity[5 + index] = digits[(hash >> ((15 - index) * 4)) & 15];
    return identity;
}
}

const std::vector<Model>& Models()
{
    static const auto models = [] {
        const auto resources = Resources();
        std::vector<Model> result;
        result.reserve(sizeof(entries) / sizeof(*entries));
        for (const auto& entry : entries)
            result.push_back({entry.id, entry.language, entry.name, entry.sourceName,
                entry.sizeKiB, entry.bytes, resources / entry.filename, entry.projectTrieId});
        return result;
    }();
    return models;
}
const Model* Find(const std::string& language, uint32_t sizeKiB)
{
    for (const auto& model : Models())
        if (model.language == language && model.sizeKiB == sizeKiB) return &model;
    return nullptr;
}
std::vector<uint8_t> Read(const Model& model)
{
    constexpr size_t maximum = 16u * 1024u * 1024u;
    if (model.bytes < 20 || model.bytes > maximum)
        throw std::runtime_error("Bundled trie metadata has an unsupported file size");
    std::ifstream file(model.path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Bundled trie is missing: " + model.path.filename().u8string());
    if (file.tellg() != static_cast<std::streamoff>(model.bytes))
        throw std::runtime_error("Bundled trie size does not match its catalog: " + model.sourceName);
    std::vector<uint8_t> bytes(model.bytes);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file || std::memcmp(bytes.data(), "NGTR", 4) || Read32(bytes.data() + 4) != 3 ||
        Read32(bytes.data() + 12) != 0)
        throw std::runtime_error("Bundled trie is not a complete compact NGTR v3 model: " + model.sourceName);
    const auto nodes = Read32(bytes.data() + 8);
    if (nodes < 2 || nodes > (bytes.size() - 16) / 2 || bytes.size() - 16 > uint64_t(nodes) * 8 ||
        Identity(bytes) != model.projectTrieId)
        throw std::runtime_error("Bundled trie content does not match its catalog: " + model.sourceName);
    return bytes;
}
}
