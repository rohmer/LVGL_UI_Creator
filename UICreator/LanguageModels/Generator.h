#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace LanguageModels {
struct Options {
    std::vector<std::filesystem::path> inputs;
    std::filesystem::path trieOutput, jsonOutput, wordListPath, cacheDirectory, wordListDirectory;
    std::string language = "en";
    std::optional<std::uint64_t> maximumBytes = 262144;
    std::array<std::optional<std::uint64_t>, 3> limits{};
    unsigned workers = (std::max)(1u, std::thread::hardware_concurrency());
    bool recursive = true, useCache = true, lowMemory = true, useWordList = true, compact = true;
    // Limits each external-sort/counting chunk, rather than the corpus size.
    std::size_t chunkEntries = 65536;
};
struct Progress {
    std::string stage;
    unsigned percent = 0;
    std::uint64_t filesProcessed = 0, filesTotal = 0, bytesProcessed = 0;
};
struct Result {
    std::array<std::uint64_t, 3> counts{};
    std::uint64_t trieBytes = 0, files = 0, cacheHits = 0;
    std::filesystem::path output;
};
class Cancelled : public std::runtime_error {
public: Cancelled() : std::runtime_error("Language model generation stopped.") {}
};
using ProgressCallback = std::function<void(const Progress&)>;
Result Generate(const Options& options, ProgressCallback progress = {}, const std::atomic_bool* cancelled = nullptr);
}
