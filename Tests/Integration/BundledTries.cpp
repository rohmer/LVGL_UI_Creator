#include "../../UICreator/KeyboardModels/BundledTries.h"
#include <Serialization/ProjectTries.h>
#include <Serialization/AdvancedKeyboard.h>
#include <Serialization/Protobuf.h>
#include <Serialization/Base64.h>
#include <lv_advanced_keyboard_prediction.h>
#include <SDL3/SDL.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <memory>
#include <set>
#include <string_view>
#include <stdexcept>

namespace {
using json = nlohmann::json;
namespace fs = std::filesystem;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
uint32_t Read32(const uint8_t* bytes)
{
    return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}
template<class Work> void Reject(Work work, const char* message)
{
    bool rejected = false;
    try { work(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, message);
}
void Write(const fs::path& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Check(bool(file), "Cannot write isolated catalog corruption fixture");
}
json Keyboard(const std::string& id, uint32_t sizeKiB = 0, const std::string& language = "en-US")
{
    return {{"advancedKeyboard", {{"language", language}, {"trieId", id}, {"trieSizeKiB", sizeKiB}}}};
}
}

int main(int argc, char** argv)
{
    try {
        Check(argc == 2, "Expected isolated bundled trie work directory");
        const auto work = fs::u8path(argv[1]); fs::create_directories(work);
        for (const uint32_t size : {0u, 16u, 256u}) {
            const auto normalized = Serialization::AdvancedKeyboard::Normalize({{"language", "fr-FR"}, {"trieSizeKiB", size}});
            Check(normalized.at("trieSizeKiB") == size, "Supported trie-size option was not preserved");
        }
        for (const auto& invalid : json::array({-1, 1, 64, 512, 1.5, "16", true}))
            Reject([&] { Serialization::AdvancedKeyboard::Normalize({{"trieSizeKiB", invalid}}); }, "Unsupported trie-size option was accepted");
        const auto& models = BundledTries::Models();
        Check(models.size() == 16, "Bundled catalog does not contain sixteen supplied models");
        std::set<std::string> languages, identifiers;
        json all = json::array();
        for (const auto& model : models) {
            Check(identifiers.insert(model.id).second, "Catalog IDs are not unique");
            languages.insert(model.language);
            Check(model.sizeKiB == 16 || model.sizeKiB == 256, "Catalog has an unknown size option");
            Check(model.bytes >= 20 && model.bytes <= 16 * 1024 * 1024,
                "Catalog has an invalid exact payload length");
            Check(model.path.filename() == fs::path(model.language + "-" + std::to_string(model.sizeKiB) + ".ngtr"),
                "Catalog resource name is not portable");
            Check(fs::equivalent(model.path.parent_path(), fs::u8path(SDL_GetBasePath()) / "language-models/bundled"),
                "Test did not resolve staged resources beside its executable");
            const auto bytes = BundledTries::Read(model);
            Check(bytes.size() == model.bytes && BundledTries::Find(model.language, model.sizeKiB) == &model,
                "Catalog selection or exact-byte read changed");
            Check(bytes.size() >= 16 && !std::memcmp(bytes.data(), "NGTR", 4) && Read32(bytes.data() + 4) == 4,
                "Bundled model was not migrated to NGTR v4");
            Check(Read32(bytes.data() + 8) < Read32(bytes.data() + 12),
                "Bundled model did not retain compressed physical and logical node counts");
            void* generatedIndex = nullptr; size_t indexSize = 0;
            Check(lv_advanced_keyboard_build_trie_index(bytes.data(), bytes.size(), &generatedIndex, &indexSize),
                "Bundled radix model could not build its canonical native index");
            std::unique_ptr<void, decltype(&lv_advanced_keyboard_free_trie_index)> index(
                generatedIndex, &lv_advanced_keyboard_free_trie_index);
            const auto* indexBytes = static_cast<const uint8_t*>(index.get());
            Check(indexSize >= 16 && !std::memcmp(indexBytes, "NGIX", 4) && Read32(indexBytes + 4) == 2 &&
                Read32(indexBytes + 8) == bytes.size() && Read32(indexBytes + 12) == Read32(bytes.data() + 8),
                "Bundled radix model produced mismatched NGIX v2 metadata");
            Check(lv_advanced_keyboard_validate_trie(bytes.data(), bytes.size(), index.get(), indexSize),
                "Bundled NGTR v4 and canonical NGIX v2 failed full native validation");
            std::string imported;
            auto single = Serialization::ProjectTries::Import(json::array(), model.name, bytes.data(), bytes.size(), &imported);
            Check(single.size() == 1 && imported == model.projectTrieId && single[0].contains("indexData"),
                "Supplied model did not import with its expected identity and index");
            Check(single[0]["indexData"] == base64_encode(indexBytes, static_cast<unsigned int>(indexSize)),
                "Project import did not retain the canonical NGIX v2 index");
            std::string duplicateId;
            const auto duplicate = Serialization::ProjectTries::Import(single, "same bytes", bytes.data(), bytes.size(), &duplicateId);
            Check(duplicate == single && duplicateId == imported, "Bundled model import duplicated shared bytes");
            auto* predictor = lv_advanced_keyboard_predictor_create_borrowed(bytes.data(), bytes.size(), index.get(), indexSize);
            Check(predictor, "Supplied compact model cannot create a predictor");
            Check(lv_advanced_keyboard_predictor_node_count(predictor) == Read32(bytes.data() + 12),
                "Bundled predictor exposed physical edge count instead of logical node count");
            lv_advanced_keyboard_suggestion_t suggestions[3]{};
            const auto count = lv_advanced_keyboard_predictor_suggest_folded(predictor, nullptr, 0,
                nullptr, 0, nullptr, 0, suggestions, 3);
            if (model.language == "zh-CN") {
                const auto contextual = lv_advanced_keyboard_predictor_suggest_folded(predictor, nullptr, 0,
                    u8"世", 3, u8"全", 3, suggestions, 3);
                Check(contextual > 0 && std::string_view(suggestions[0].text, suggestions[0].length) == u8"界",
                    "Mandarin model did not back off to its real next-character bigram");
            }
            lv_advanced_keyboard_predictor_destroy(predictor);
            Check(count > 0, "Supplied model has no usable unigram predictions");
            all.push_back(single[0]);
        }
        Check(languages == std::set<std::string>({"en-US", "es-ES", "fr-FR", "de-DE", "el-GR", "ar-SA", "hi-IN", "zh-CN"}),
            "Bundled corpus locales do not match keyboard locales");
        Check(BundledTries::Find("es-ES", 16)->sourceName == "esMS16.ngtr" &&
            BundledTries::Find("ar-SA", 16)->sourceName == "arAR16.ngtr" &&
            BundledTries::Find("hi-IN", 16)->sourceName == "hiHI16.ngtr" &&
            BundledTries::Find("el-GR", 16)->sourceName == "elEL16.ngtr" &&
            BundledTries::Find("zh-CN", 16)->sourceName == "zhZH16.ngtr",
            "Original corpus aliases were discarded");
        Check(!BundledTries::Find("bn-BD", 256) &&
            !BundledTries::Find("en-US", 64), "Catalog fabricated an unavailable model or size");

        auto damaged = models.front(); damaged.path = work / "changed.ngtr";
        auto bytes = BundledTries::Read(models.front()); bytes.back() ^= 1; Write(damaged.path, bytes);
        Reject([&] { BundledTries::Read(damaged); }, "Modified model content was accepted");
        Write(damaged.path, {'N', 'G', 'T', 'R'});
        Reject([&] { BundledTries::Read(damaged); }, "Truncated model was accepted");
        fs::remove(damaged.path);
        Reject([&] { BundledTries::Read(damaged); }, "Missing bundled model was accepted");

        const auto* selected = BundledTries::Find("zh-CN", 16);
        json document = {{"canvas", {{"width", 320}, {"height", 240}}}, {"tries", all},
            {"obj", json::array({Keyboard(selected->projectTrieId, selected->sizeKiB, selected->language), Keyboard(selected->projectTrieId, selected->sizeKiB, selected->language)})}};
        Serialization::ProjectTries::PruneUnused(document);
        Check(document["tries"].size() == 1 && document["tries"][0]["id"] == selected->projectTrieId,
            "Generated project retained unreferenced models or duplicated a shared selection");
        const auto wire = Serialization::Protobuf::Encode(Serialization::EntityKind::Project, "project", document);
        const auto decoded = Serialization::Protobuf::Decode(wire);
        Check(decoded.data.at("tries").size() == 1 && decoded.data.at("tries")[0].at("data").is_binary() &&
            decoded.data.at("tries")[0].at("indexData").is_binary(), "Chosen model and index were not embedded as binary");
        Check(decoded.data.at("obj")[0].at("advancedKeyboard").at("trieSizeKiB") == selected->sizeKiB,
            "Selected trie-size metadata did not round-trip through Protobuf");
        Check(decoded.data.at("obj")[0].at("advancedKeyboard").at("language") == "zh-CN",
            "Mandarin model language did not round-trip through Protobuf");
        Check(wire.size() < selected->bytes * 3 + 4096, "Generated payload bundled the full corpus library");
        auto missing = document;
        missing["obj"][0]["advancedKeyboard"]["trieId"] = "missing-model";
        const auto before = missing;
        Reject([&] { Serialization::ProjectTries::PruneUnused(missing); }, "Unknown selected model was accepted");
        Check(missing == before, "Rejecting an unknown selection mutated the project");
        document["tries"] = all;
        document["obj"] = json::array({Keyboard("builtin-en-us"), Keyboard("")});
        Serialization::ProjectTries::PruneUnused(document);
        Check(!document.contains("tries"), "Built-in/None selections bundled external corpus blobs");
        std::cout << "Sixteen bundled NGTR v4/NGIX v2 models: validated predictors, exact bytes, aliases, dedup, corruption rejection, and selected-only project embedding passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
