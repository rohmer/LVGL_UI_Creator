#include "LanguageModelSmoke.h"
#include <Generator.h>
#include "../../3rdParty/JSON/json.hpp"
#include <lv_advanced_keyboard_prediction.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string Word(const lv_advanced_keyboard_suggestion_t& suggestion) {
    return {suggestion.text, suggestion.length};
}
struct DestroyPredictor {
    void operator()(lv_advanced_keyboard_predictor_t* predictor) const {
        lv_advanced_keyboard_predictor_destroy(predictor);
    }
};
}

int RunLanguageModelSmoke(const std::filesystem::path& destination) {
    try {
        namespace fs = std::filesystem;
        const auto output = fs::absolute(destination);
        Require(!fs::exists(output) || (fs::is_directory(output) && fs::is_empty(output)),
                "Language model smoke destination must be a new or empty directory.");
        fs::create_directories(output);
        const auto corpus = output / "corpus.txt";
        {
            std::ofstream stream(corpus, std::ios::binary);
            for (unsigned repeat = 0; repeat < 1000; ++repeat)
                stream << "the world. the world. the world.\n"
                          "hello world. hello there. help the hero.\n"
                          "in the world. in the world.\n";
            stream.close();
            Require(bool(stream), "Cannot write language model smoke corpus.");
        }
        LanguageModels::Options options;
        options.inputs = {corpus};
        options.trieOutput = output / "model.ngtr";
        options.jsonOutput = output / "model.json";
        options.useWordList = false;
        options.useCache = false;
        options.lowMemory = false;
        options.workers = 1;
        const auto generated = LanguageModels::Generate(options);
        std::ifstream input(options.trieOutput, std::ios::binary);
        Require(bool(input), "Cannot read generated language model.");
        const std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(input), {});
        Require(bytes.size() == generated.trieBytes && !bytes.empty(), "Generated trie size is invalid.");
        std::unique_ptr<lv_advanced_keyboard_predictor_t, DestroyPredictor> predictor(
            lv_advanced_keyboard_predictor_create(bytes.data(), bytes.size()));
        Require(bool(predictor), "Keyboard predictor rejected the generated trie.");
        std::array<lv_advanced_keyboard_suggestion_t, 3> suggestions{};
        const auto count = lv_advanced_keyboard_predictor_suggest_folded(
            predictor.get(), "hel", 3, nullptr, 0, nullptr, 0, suggestions.data(), suggestions.size());
        Require(count == 2 && Word(suggestions[0]) == "hello" && Word(suggestions[1]) == "help",
                "Generated trie failed ranked prefix completion.");
        const auto contextual = lv_advanced_keyboard_predictor_suggest_folded(
            predictor.get(), "wo", 2, "the", 3, "in", 2, suggestions.data(), suggestions.size());
        Require(contextual && Word(suggestions[0]) == "world", "Generated trie failed context prediction.");
        lv_advanced_keyboard_suggestion_t correction{};
        double confidence = 0;
        Require(lv_advanced_keyboard_predictor_autocorrect_utf8(
                    predictor.get(), "teh", 3, nullptr, 0, nullptr, 0, 0.3, &correction, &confidence)
                    && Word(correction) == "the" && confidence >= 0.3,
                "Generated trie failed keyboard autocorrection.");
        const nlohmann::json report = {
            {"status", "passed"}, {"pointerBits", sizeof(void*) * 8},
            {"trieBytes", generated.trieBytes}, {"language", options.language},
            {"prefixSuggestions", {"hello", "help"}}, {"contextSuggestion", "world"},
            {"autocorrection", "the"}, {"confidence", confidence}
        };
        std::ofstream result(output / "report.json", std::ios::binary);
        result << report.dump(2) << '\n';
        result.close();
        Require(bool(result), "Cannot write language model smoke report.");
        std::printf("Language model generation, prediction and autocorrection passed (%zu-bit).\n", sizeof(void*) * 8);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Language model smoke failed: %s\n", error.what());
        return EXIT_FAILURE;
    }
}
