#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace BundledTries {
struct Model {
    std::string id, language, name, sourceName;
    uint32_t sizeKiB = 0;
    size_t bytes = 0;
    std::filesystem::path path;
    std::string projectTrieId;
};

// Metadata only: model bytes are loaded when selected, then embedded in the project.
const std::vector<Model>& Models();
const Model* Find(const std::string& language, uint32_t sizeKiB);
// Reject missing, truncated or modified resources before changing an open project.
std::vector<uint8_t> Read(const Model& model);
}
