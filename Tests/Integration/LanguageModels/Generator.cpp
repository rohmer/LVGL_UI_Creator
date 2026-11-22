#include "../../../UICreator/LanguageModels/Generator.h"
#include "../../../3rdParty/JSON/json.hpp"
#include <lv_advanced_keyboard_prediction.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iostream>
#include <locale>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using json = nlohmann::json;
using Bytes = std::vector<uint8_t>;
void Check(bool valid, const std::string& message) { if (!valid) throw std::runtime_error(message); }
Bytes Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    Check(bool(input), "Cannot read " + path.u8string());
    return Bytes(std::istreambuf_iterator<char>(input), {});
}
void Write(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    Check(bool(output), "Cannot create " + path.u8string());
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    Check(bool(output), "Cannot write " + path.u8string());
}
json ReadJson(const fs::path& path) { std::ifstream input(path, std::ios::binary); return json::parse(input); }
void CompareJson(const json& actual, const json& expected, const std::string& scenario) {
    for (const auto* order : {"unigrams", "bigrams", "trigrams"}) {
        Check(actual.contains(order) && actual.at(order).size() == expected.at(order).size(), scenario + ": n-gram count differs");
        for (size_t i = 0; i < expected.at(order).size(); ++i) {
            const auto& first = actual.at(order).at(i); const auto& second = expected.at(order).at(i);
            Check(first.at("tokens") == second.at("tokens") && first.at("count") == second.at("count"), scenario + ": n-gram ranking/count differs from C#");
            Check(std::abs(first.at("weight").get<double>() - second.at("weight").get<double>()) < 1e-12, scenario + ": prefix probability differs from C#");
        }
    }
}
template<class Action> void Reject(Action action, const std::string& scenario) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    Check(failed, scenario + ": invalid generation request was accepted");
}
LanguageModels::Options Base(const fs::path& fixture, const fs::path& work, const std::string& name) {
    LanguageModels::Options options;
    options.inputs = {fixture / "ascii.txt"};
    options.trieOutput = work / (name + ".ngtr"); options.jsonOutput = work / (name + ".json");
    options.maximumBytes.reset(); options.useWordList = false; options.useCache = false;
    options.lowMemory = false; options.workers = 1; options.compact = true;
    return options;
}
std::string Word(const lv_advanced_keyboard_suggestion_t& value) { return {value.text, value.length}; }
struct DestroyPredictor { void operator()(lv_advanced_keyboard_predictor_t* value) const { lv_advanced_keyboard_predictor_destroy(value); } };
void Runtime(const Bytes& data, bool expectCorrection = false) {
    std::unique_ptr<lv_advanced_keyboard_predictor_t, DestroyPredictor> predictor(lv_advanced_keyboard_predictor_create(data.data(), data.size()));
    Check(bool(predictor), "Generated compact model cannot be loaded by the device predictor");
    std::array<lv_advanced_keyboard_suggestion_t, 3> words{};
    const auto prefix = lv_advanced_keyboard_predictor_suggest_folded(predictor.get(), "hel", 3, nullptr, 0, nullptr, 0, words.data(), words.size());
    Check(prefix == 2 && Word(words[0]) == "hello" && Word(words[1]) == "help", "Generated model did not supply real ranked prefix completions");
    Check(lv_advanced_keyboard_predictor_suggest_folded(predictor.get(), "wo", 2, "the", 3, "in", 2, words.data(), words.size()) && Word(words[0]) == "world",
          "Generated model did not supply trigram context suggestions");
    if (expectCorrection) {
        lv_advanced_keyboard_suggestion_t correction{}; double probability = 0;
        Check(lv_advanced_keyboard_predictor_autocorrect_utf8(predictor.get(), "teh", 3, nullptr, 0, nullptr, 0, 0.3, &correction, &probability) && Word(correction) == "the" && probability >= 0.3,
              "Generated model did not supply a real transposition correction");
    }
}
void ReferenceCases(const fs::path& fixture, const fs::path& work, const fs::path& words) {
    for (const auto* name : {"full", "legacy", "budget128", "budget20", "limits", "zero", "folded", "es", "fr", "de", "el"}) {
        std::cout << "Generating C# parity case " << name << '\n';
        auto options = Base(fixture, work, name);
        const std::string scenario = name;
        if (scenario == "legacy") options.compact = false;
        if (scenario == "budget128") options.maximumBytes = 128;
        if (scenario == "budget20") options.maximumBytes = 20;
        if (scenario == "limits") options.limits = {3, 2, 1};
        if (scenario == "zero") options.limits = {0, 0, 0};
        if (scenario == "folded") options.inputs = {fixture / "folded.txt"};
        if (scenario == "es" || scenario == "fr" || scenario == "de" || scenario == "el") {
            options.language = name; options.inputs = {fixture / (scenario + ".txt")};
            options.useWordList = true; options.wordListDirectory = words;
        }
        const auto result = LanguageModels::Generate(options);
        const auto expected = ReadJson(fixture / (scenario + ".json"));
        const auto bytes = Read(options.trieOutput);
        Check(bytes == Read(fixture / (scenario + ".ngtr")), scenario + ": native NGTR bytes differ from the original C# generator");
        CompareJson(ReadJson(options.jsonOutput), expected, scenario);
        Check(result.files == 1 && result.trieBytes == bytes.size() && result.output == options.trieOutput, scenario + ": generation result metadata differs");
        for (size_t order = 0; order < 3; ++order)
            Check(result.counts[order] == expected.at(std::array<const char*, 3>{"unigrams", "bigrams", "trigrams"}[order]).size(), scenario + ": result n-gram totals differ");
        if (options.maximumBytes) Check(bytes.size() <= *options.maximumBytes, scenario + ": model exceeds its byte budget");
        if (scenario == "full") Runtime(bytes);
        if (options.compact) {
            void* index = nullptr; size_t indexBytes = 0;
            Check(lv_advanced_keyboard_build_trie_index(bytes.data(), bytes.size(), &index, &indexBytes), scenario + ": compact model cannot produce a device index");
            const bool valid = lv_advanced_keyboard_validate_trie(bytes.data(), bytes.size(), index, indexBytes);
            lv_advanced_keyboard_free_trie_index(index);
            Check(valid, scenario + ": device runtime rejects generated model");
        }
        std::cout << "C# parity " << name << ": " << bytes.size() << " bytes\n";
    }
}
void ModesAndEncoding(const fs::path& fixture, const fs::path& work) {
    const auto expected = Read(fixture / "full.ngtr");
    for (bool disk : {false, true}) for (unsigned workers : {1u, 4u}) {
        std::cout << "Counting mode " << (disk ? "disk" : "memory") << ", workers " << workers << '\n';
        auto options = Base(fixture, work, std::string(disk ? "disk" : "memory") + std::to_string(workers));
        options.lowMemory = disk; options.chunkEntries = 2; options.workers = workers;
        std::mutex mutex; unsigned last = 0, callbacks = 0;
        LanguageModels::Generate(options, [&](const LanguageModels::Progress& progress) {
            std::lock_guard<std::mutex> lock(mutex);
            Check(progress.percent >= last && progress.percent <= 100 && !progress.stage.empty(), "Progress is invalid or moves backwards");
            last = progress.percent; ++callbacks;
        });
        Check(callbacks > 1 && last == 100 && Read(options.trieOutput) == expected, "Disk/memory or worker count changed generated model");
    }
    const auto input = Read(fixture / "ascii.txt");
    // The scorer deliberately prefers keeping a typo over very infrequent
    // alternatives. Give it realistic training counts rather than changing the
    // device heuristic to satisfy the tiny byte-parity corpus.
    std::string frequent;
    for (unsigned repeat = 0; repeat < 1000; ++repeat) frequent.append(input.begin(), input.end());
    auto trained = Base(fixture, work, "trained-correction");
    std::cout << "Training realistic autocorrection counts\n";
    trained.inputs = {work / "trained-correction.txt"}; Write(trained.inputs[0], frequent);
    LanguageModels::Generate(trained); Runtime(Read(trained.trieOutput), true);
    for (unsigned width : {2u, 4u}) for (bool big : {false, true}) {
        std::cout << "Checking encoding " << width << (big ? "be" : "le") << '\n';
        std::string encoded;
        if (width == 2) encoded = big ? std::string("\xfe\xff", 2) : std::string("\xff\xfe", 2);
        else encoded = big ? std::string("\0\0\xfe\xff", 4) : std::string("\xff\xfe\0\0", 4);
        for (uint8_t byte : input) for (unsigned index = 0; index < width; ++index) encoded += char(index == (big ? width - 1 : 0) ? byte : 0);
        auto options = Base(fixture, work, "encoding" + std::to_string(width) + (big ? "be" : "le"));
        const auto path = options.trieOutput.string() + ".txt";
        Write(path, encoded); options.inputs = {path}; LanguageModels::Generate(options);
        Check(Read(options.trieOutput) == expected, "BOM-encoded corpus changed n-gram counts");
    }
    auto options = Base(fixture, work, "utf8-bom");
    Write(work / "bom.txt", std::string("\xef\xbb\xbf", 3) + std::string(input.begin(), input.end()));
    options.inputs = {work / "bom.txt"}; LanguageModels::Generate(options);
    Check(Read(options.trieOutput) == expected, "UTF-8 BOM became part of a corpus token");
    struct DecimalComma : std::numpunct<char> {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
    };
    const auto originalLocale = std::locale();
    try {
        std::locale::global(std::locale(originalLocale, new DecimalComma));
        options = Base(fixture, work, "localized-host"); LanguageModels::Generate(options);
        CompareJson(ReadJson(options.jsonOutput), ReadJson(fixture / "full.json"), "Non-English host locale");
        Check(Read(options.trieOutput) == expected, "Host locale changed binary generation");
    } catch (...) { std::locale::global(originalLocale); throw; }
    std::locale::global(originalLocale);
}
void CacheAndDiscovery(const fs::path& fixture, const fs::path& work) {
    const auto folder = work / "corpus"; fs::create_directories(folder / "nested");
    const auto source = Read(fixture / "ascii.txt");
    Write(folder / "first.txt", std::string(source.begin(), source.end()));
    Write(folder / "nested/second.TXT", "hello world.\n"); Write(folder / "ignored.bin", "ignored words\n");
    auto options = Base(fixture, work, "cache");
    options.inputs = {folder}; options.lowMemory = true; options.chunkEntries = 2;
    options.useCache = true; options.cacheDirectory = work / "cache";
    const auto first = LanguageModels::Generate(options); const auto firstBytes = Read(options.trieOutput);
    Check(first.files == 2 && first.cacheHits == 0, "Directory discovery or initial cache statistics differ");
    const auto second = LanguageModels::Generate(options);
    Check(second.cacheHits == 2 && Read(options.trieOutput) == firstBytes, "Unchanged corpora were not reused correctly");
    Write(folder / "first.txt", "hello hello hello.\n");
    const auto third = LanguageModels::Generate(options);
    Check(third.cacheHits == 1 && Read(options.trieOutput) != firstBytes, "Changed corpus reused stale counts");
    for (const auto& item : fs::directory_iterator(options.cacheDirectory)) if (item.is_regular_file()) Write(item.path(), "corrupt cache");
    const auto rebuilt = LanguageModels::Generate(options);
    Check(rebuilt.cacheHits == 0, "Corrupt cache was treated as a valid count stream");
    options.recursive = false; options.useCache = false;
    Check(LanguageModels::Generate(options).files == 1, "Nonrecursive corpus search included a nested file");
    auto filtered = Base(fixture, work, "filtered");
    filtered.inputs = {work / "filter-corpus.txt"}; filtered.useWordList = true;
    filtered.wordListPath = work / "custom-words.txt"; filtered.useCache = true; filtered.cacheDirectory = work / "filter-cache";
    Write(filtered.inputs[0], "the wug world\n"); Write(filtered.wordListPath, "the\nworld\n");
    LanguageModels::Generate(filtered);
    auto result = ReadJson(filtered.jsonOutput);
    Check(result["unigrams"].size() == 2 && result["bigrams"].empty(), "Rejected word did not reset n-gram context");
    Write(filtered.wordListPath, "world\n");
    Check(LanguageModels::Generate(filtered).cacheHits == 0 && ReadJson(filtered.jsonOutput)["unigrams"].size() == 1,
          "Changed custom word list reused stale filtered counts");
}
void InvalidAndCancellation(const fs::path& fixture, const fs::path& work) {
    auto original = Base(fixture, work, "protected-output");
    Write(original.trieOutput, "preserve destination"); const auto before = Read(original.trieOutput);
    auto reject = [&](LanguageModels::Options options, const char* message) {
        Reject([&] { LanguageModels::Generate(options); }, message);
        Check(Read(original.trieOutput) == before, std::string(message) + ": validation damaged the existing output");
    };
    auto options = original; options.inputs.clear(); reject(options, "Empty corpus inputs");
    options = original; options.inputs = {work / "missing.txt"}; reject(options, "Missing corpus");
    options = original; options.maximumBytes = 19; reject(options, "Undersized v3 budget");
    options = original; options.compact = false; options.maximumBytes = 39; reject(options, "Undersized v2 budget");
    options = original; options.workers = 0; reject(options, "Zero workers");
    options = original; options.language = "unsupported"; reject(options, "Unsupported corpus language");
    options = original; options.inputs = {work / "alias-corpus.txt"};
    const auto corpusCopy = Read(fixture / "ascii.txt");
    Write(options.inputs[0], std::string(corpusCopy.begin(), corpusCopy.end()));
    options.trieOutput = options.inputs[0];
    const auto corpusBefore = Read(options.inputs[0]); Reject([&] { LanguageModels::Generate(options); }, "Corpus/output alias");
    Check(Read(options.inputs[0]) == corpusBefore, "Output alias damaged source corpus");
    options = original; options.jsonOutput = options.trieOutput; reject(options, "Equal JSON/trie outputs");
    options = original; options.inputs = {work / "invalid.txt"}; Write(options.inputs[0], std::string("\xc3(", 2)); reject(options, "Malformed UTF-8 corpus");
    options = original; options.useWordList = true; options.wordListPath = work / "invalid-words.txt";
    Write(options.wordListPath, "two words\n"); reject(options, "Malformed custom word list");
    std::atomic_bool cancel{true}; bool cancelled = false;
    try { LanguageModels::Generate(original, {}, &cancel); } catch (const LanguageModels::Cancelled&) { cancelled = true; }
    Check(cancelled && Read(original.trieOutput) == before, "Pre-cancelled generation wrote output or failed with the wrong error");
    cancel = false; cancelled = false;
    options = original; options.inputs = {work / "cancel-corpus.txt"}; options.lowMemory = true; options.chunkEntries = 256;
    // Reach the byte-progress callback after real external counting work,
    // without creating tens of thousands of two-entry files first on NTFS.
    // Unique tokens force spills even with the larger bounded chunk size.
    std::string large; for (unsigned i = 0; i < 10000; ++i) large += "cancelword" + std::to_string(i) + " ";
    Write(options.inputs[0], large);
    std::vector<fs::path> existingDirectories;
    for (const auto& entry : fs::directory_iterator(work)) if (entry.is_directory()) existingDirectories.push_back(entry.path());
    size_t spilledFiles = 0;
    try { LanguageModels::Generate(options, [&](const LanguageModels::Progress& progress) {
        if (progress.stage != "Counting n-grams" || !progress.bytesProcessed || progress.percent >= 80) return;
        Check(progress.bytesProcessed < large.size(), "Cancellation did not occur before counting completed");
        for (const auto& entry : fs::directory_iterator(work)) {
            if (!entry.is_directory() || std::find(existingDirectories.begin(), existingDirectories.end(), entry.path()) != existingDirectories.end()) continue;
            for (const auto& chunk : fs::directory_iterator(entry.path())) if (chunk.is_regular_file()) ++spilledFiles;
        }
        Check(spilledFiles > 0, "Cancellation fixture did not exercise external spill storage");
        std::cout << "Cancelling after " << progress.bytesProcessed << " corpus bytes and " << spilledFiles << " external count chunks\n";
        cancel = true;
    }, &cancel); }
    catch (const LanguageModels::Cancelled&) { cancelled = true; }
    Check(cancelled && cancel && spilledFiles && Read(original.trieOutput) == before && !fs::exists(original.jsonOutput),
          "In-flight disk cancellation failed or published partial output");
}
}
int main(int argc, char** argv) {
    try {
        std::cout << std::unitbuf;
        Check(argc == 4, "Expected fixture, work and bundled word-list directories");
        const auto fixture = fs::absolute(fs::u8path(argv[1]));
        const auto work = fs::absolute(fs::u8path(argv[2])) / ("run-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(work);
        ReferenceCases(fixture, work, fs::u8path(argv[3]));
        std::cout << "Starting counting modes and encoding cases\n";
        ModesAndEncoding(fixture, work);
        std::cout << "Starting cache and corpus discovery cases\n";
        CacheAndDiscovery(fixture, work);
        std::cout << "Starting invalid input and cancellation cases\n";
        InvalidAndCancellation(fixture, work);
        std::cout << "Native C# parity, Unicode, budgets, device prediction, disk/worker/cache modes, encodings, validation and cancellation passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
