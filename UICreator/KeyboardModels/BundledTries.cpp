#include "BundledTries.h"
#include "../../Integrations/AdvancedKeyboard/Paths.h"
#include "../../3rdParty/JSON/json.hpp"
#include <SDL3/SDL.h>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace BundledTries {
namespace {
std::filesystem::path Resources()
{
    const auto* executable = SDL_GetBasePath();
    if (executable) {
        auto installed = std::filesystem::u8path(executable) / "language-models" / "bundled";
        if (std::filesystem::is_directory(installed)) return installed;
    }
    // Development and isolated tests can use the checked-in resource copy.
    auto source = AdvancedKeyboardDependency::SourceRoot() / "models" / "bundled" / "models";
    if (std::filesystem::is_directory(source)) return source;
    return executable ? std::filesystem::u8path(executable) / "language-models" / "bundled" : source;
}
uint32_t Read32(const uint8_t* value)
{
    return uint32_t(value[0]) | (uint32_t(value[1]) << 8) |
        (uint32_t(value[2]) << 16) | (uint32_t(value[3]) << 24);
}
bool ValidHeader(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < 20 || std::memcmp(bytes.data(), "NGTR", 4)) return false;
    const auto version = Read32(bytes.data() + 4), nodes = Read32(bytes.data() + 8), extra = Read32(bytes.data() + 12);
    if (nodes < 2) return false;
    if (version == 2) return extra == 12 && bytes.size() == 16 + uint64_t(nodes) * 12;
    if (nodes > (bytes.size() - 16) / 2) return false;
    if (version == 3) return extra == 0 && bytes.size() - 16 <= uint64_t(nodes) * 8;
    return version == 4 && nodes <= 0x00ffffffu && extra >= nodes && extra <= 2 + uint64_t(nodes - 2) * 256;
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
        std::ifstream input(resources / "catalog.json");
        if (!input) input.open(resources.parent_path() / "catalog.json");
        if (!input) throw std::runtime_error("Bundled keyboard catalog is missing");
        nlohmann::json catalog; input >> catalog;
        if (!catalog.is_object() || catalog.value("version", 0) != 1 ||
            !catalog.contains("models") || !catalog["models"].is_array())
            throw std::runtime_error("Bundled keyboard catalog has an unsupported schema");
        std::vector<Model> result;
        result.reserve(catalog["models"].size());
        for (const auto& entry : catalog["models"]) {
            const auto filename = entry.at("filename").get<std::string>();
            if (filename.empty() || filename.find_first_of("/\\:") != std::string::npos ||
                filename == "." || filename == "..")
                throw std::runtime_error("Bundled keyboard model path is invalid");
            result.push_back({entry.at("id").get<std::string>(), entry.at("language").get<std::string>(),
                entry.at("name").get<std::string>(), entry.at("sourceName").get<std::string>(),
                entry.at("sizeKiB").get<uint32_t>(), entry.at("bytes").get<size_t>(),
                resources / filename, entry.at("projectTrieId").get<std::string>()});
        }
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
    if (!file || !ValidHeader(bytes))
        throw std::runtime_error("Bundled trie is not a complete NGTR v2, v3 or radix v4 model: " + model.sourceName);
    if (Identity(bytes) != model.projectTrieId)
        throw std::runtime_error("Bundled trie content does not match its catalog: " + model.sourceName);
    return bytes;
}
}
