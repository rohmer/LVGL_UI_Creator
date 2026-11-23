// Focused native tests; no editor window or interactive input is required.
#include "../../LVSerial/Widgets/AdvancedKeyboard/SuggestionText.h"
#include "../../LVSerial/Widgets/AdvancedKeyboard/Utf8Fold.h"
#include "../../UICreator/LanguageModels/Generator.h"
#include <lv_advanced_keyboard_prediction.h>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace Text = espos::keyboard::detail;
namespace Unicode = espos::keyboard::prediction;
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void Set(lv_obj_t* field, const char* text, int caret = LV_TEXTAREA_CURSOR_LAST) {
    lv_textarea_set_text(field, text); lv_textarea_set_cursor_pos(field, caret);
}
void Context(lv_obj_t* field) {
    Set(field, u8"你好"); auto input = Text::capture_suggestion_text(field, true);
    Check(input.prefix.empty() && input.previous == u8"好" && input.older == u8"你" && input.start == 6 && input.end == 6,
          "Committed Han characters did not become two context tokens");
    Check(Text::accept_suggestion(field, input, u8"吗", true, true) && std::string(lv_textarea_get_text(field)) == u8"你好吗" && lv_textarea_get_cursor_pos(field) == 3,
          "A Han next-character candidate inserted an ASCII space or used byte caret indices");
    Set(field, u8"你好", 1); input = Text::capture_suggestion_text(field, true);
    Check(input.prefix.empty() && input.previous == u8"你" && input.older.empty() && input.start == 3 && input.end == 3,
          "A caret between Han scalars replaced neighboring text");
    Check(Text::accept_suggestion(field, input, u8"世", true, true) && std::string(lv_textarea_get_text(field)) == u8"你世好" && lv_textarea_get_cursor_pos(field) == 2,
          "A middle-of-text Han candidate lost its UTF-8 tail");
    Set(field, u8"𠀀𠀁"); input = Text::capture_suggestion_text(field, true);
    Check(input.previous == u8"𠀁" && input.older == u8"𠀀" && input.start == 8 && input.prefix.empty(),
          "Supplementary Han used UTF-16 or byte token boundaries");
    Set(field, u8"你好hel"); input = Text::capture_suggestion_text(field, true);
    Check(input.prefix == "hel" && input.previous == u8"好" && input.older == u8"你", "Mixed Latin prefixes swallowed Han context");
    Check(Text::accept_suggestion(field, input, "hello", true, true) && std::string(lv_textarea_get_text(field)) == u8"你好hello",
          "A Mandarin Latin completion did not replace its ordinary word");
    Set(field, u8"hello你"); input = Text::capture_suggestion_text(field, true);
    Check(input.prefix.empty() && input.previous == u8"你" && input.older == "hello", "Mixed Latin/Han context lost the Latin word");
    Set(field, u8"你。好"); input = Text::capture_suggestion_text(field, true);
    Check(input.previous == u8"好" && input.older.empty(), "Chinese punctuation did not reset prior context");
}
void Scripts(lv_obj_t* field) {
    struct Case { const char* text; const char* prefix; const char* previous; const char* older; };
    for (const auto& item : std::array<Case, 6>{{
        {u8"مرحبا عالم سل", u8"سل", u8"عالم", u8"مرحبا"},
        {u8"नमस्ते दुनिया सु", u8"सु", u8"दुनिया", u8"नमस्ते"},
        {u8"বাংলা ভাষা সু", u8"সু", u8"ভাষা", u8"বাংলা"},
        {u8"اردو زبان پا", u8"پا", u8"زبان", u8"اردو"},
        {u8"привет мир пр", u8"пр", u8"мир", u8"привет"},
        {"selamat dunia sa", "sa", "dunia", "selamat"},
    }}) {
        Set(field, item.text); const auto input = Text::capture_suggestion_text(field);
        Check(input.prefix == item.prefix && input.previous == item.previous && input.older == item.older,
              "A new script lost its typed prefix or its two prior words");
    }
    Set(field, u8"क्‍ष क्‌ष सु"); auto input = Text::capture_suggestion_text(field);
    Check(input.previous == u8"क्‌ष" && input.older == u8"क्‍ष", "Internal Indic joiners or combining marks were dropped");
    Set(field, u8"عَالَم سل"); input = Text::capture_suggestion_text(field);
    Check(input.previous == u8"عَالَم", "Arabic vowel marks were omitted from previous context");
    Set(field, u8"привет мир ПР"); input = Text::capture_suggestion_text(field);
    std::array<char, 32> output{};
    const auto size = Text::display_suggestion(input, u8"привет", output.data(), output.size());
    Check(std::string_view(output.data(), size) == u8"ПРИВЕТ", "Russian all-caps completion changed casing or UTF-8 width");
    Set(field, "hello world he"); input = Text::capture_suggestion_text(field);
    Check(input.prefix == "he" && input.previous == "world" && input.older == "hello", "Existing Latin context changed");
    Check(Text::accept_suggestion(field, input, "hello") && std::string(lv_textarea_get_text(field)) == "hello world hello ",
          "Existing Latin suggestion spacing changed");
    Check(Unicode::IsWordScalar(0x938) && Unicode::IsMarkScalar(0x941) && !Unicode::IsWordScalar(0x964) &&
          !Unicode::IsWordScalar(0x60c) && !Unicode::IsWordScalar(0x200d), "Unicode categories treated script punctuation or joiners as letters");
    Check(Unicode::UpperCyrillic(0x1c80) == 0x1c80, "An unsupported width-changing Cyrillic uppercase map was applied in place");
}
void Guards(lv_obj_t* field) {
    Set(field, u8"你好"); auto input = Text::capture_suggestion_text(field, true);
    auto broken = input; broken.start = 1;
    Check(!Text::accept_suggestion(field, broken, u8"世", true, true), "An invalid UTF-8 byte replacement boundary was accepted");
    Check(!Text::accept_suggestion(field, input, std::string_view("\xe4\xbd", 2), true, true), "A partial UTF-8 candidate was accepted");
    Check(!Text::accept_suggestion(field, input, std::string_view("a\0b", 3), true, true), "An embedded-NUL candidate was accepted");
    auto* label = lv_textarea_get_label(field);
    lv_label_set_text_selection_start(label, 0); lv_label_set_text_selection_end(label, 1);
    input = Text::capture_suggestion_text(field, true);
    Check(input.selected && !Text::accept_suggestion(field, input, u8"世", true, true), "Selected text exposed or accepted stale suggestions");
    lv_label_set_text_selection_start(label, LV_LABEL_TEXT_SELECTION_OFF); lv_label_set_text_selection_end(label, LV_LABEL_TEXT_SELECTION_OFF);
    lv_textarea_set_password_mode(field, true); input = Text::capture_suggestion_text(field, true);
    Check(input.selected && input.text.empty() && !Text::accept_suggestion(field, input, u8"世", true, true), "Password text entered the prediction context");
    lv_textarea_set_password_mode(field, false);
    Set(field, u8"你好"); input = Text::capture_suggestion_text(field, true); lv_textarea_set_cursor_pos(field, 1);
    Check(!Text::accept_suggestion(field, input, u8"世", true, true), "A stale caret snapshot was accepted");
}
void NfcProbe(lv_obj_t* field) {
    namespace fs = std::filesystem;
    struct Fixture {
        fs::path directory = fs::temp_directory_path() / ("keyboard-nfc-probe-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Fixture() { Check(fs::create_directory(directory), "Cannot create the isolated NFC probe directory"); }
        ~Fixture() { std::error_code ignored; fs::remove_all(directory, ignored); }
    } fixture;
    const auto corpus = fixture.directory / "corpus.txt";
    {
        std::ofstream output(corpus, std::ios::binary);
        output << u8"start \u0958\u0932\u092e \u09dc\u09be\u09b2\u0964";
        Check(bool(output), "Cannot write the NFC probe corpus");
    }
    LanguageModels::Options options;
    options.inputs = {corpus}; options.language = "hi";
    options.trieOutput = fixture.directory / "model.ngtr";
    options.scratchDirectory = fixture.directory / "scratch";
    options.useCache = false; options.useWordList = false; options.lowMemory = false;
    options.workers = 1; options.maximumBytes.reset(); options.compact = true;
    const auto result = LanguageModels::Generate(options);
    Check(result.counts == std::array<std::uint64_t, 3>{3, 2, 1}, "The canonical NFC probe corpus lost its context tokens");
    std::ifstream stream(options.trieOutput, std::ios::binary); std::vector<char> bytes(std::istreambuf_iterator<char>(stream), {});
    Check(!bytes.empty(), "Cannot read the NFC probe model");
    auto* predictor = lv_advanced_keyboard_predictor_create(bytes.data(), bytes.size());
    Check(predictor, "Cannot load the NFC probe model");
    for (const auto& item : std::array<std::pair<std::string_view, std::string_view>, 2>{{
        {u8"\u0958", u8"\u0915\u093c"}, {u8"\u09dc", u8"\u09a1\u09bc"}}}) {
        std::array<char, 256> storage{}; std::string_view folded;
        std::array<lv_advanced_keyboard_suggestion_t, 3> words{};
        Check(Unicode::FoldUtf8(item.first, storage.data(), storage.size(), folded), "Raw Indic prefix did not fold");
        const auto raw = lv_advanced_keyboard_predictor_suggest_folded(predictor, folded.data(), folded.size(), nullptr, 0, nullptr, 0, words.data(), words.size());
        Check(Unicode::FoldUtf8(item.second, storage.data(), storage.size(), folded), "Normalized Indic prefix did not fold");
        const auto normalized = lv_advanced_keyboard_predictor_suggest_folded(predictor, folded.data(), folded.size(), nullptr, 0, nullptr, 0, words.data(), words.size());
        Check(normalized != 0, "The model has no canonical Indic prefix fixture");
        Check(Unicode::FoldUtf8(item.first, storage.data(), storage.size(), folded, true), "Canonical Indic folding failed");
        const auto canonical = lv_advanced_keyboard_predictor_suggest_folded(predictor, folded.data(), folded.size(), nullptr, 0, nullptr, 0, words.data(), words.size());
        Check(raw == 0 && canonical == normalized, "Default folding changed or opt-in Nukta canonicalization missed a real model prefix");
        const std::string rawText(item.first);
        Set(field, rawText.c_str());
        const auto captured = Text::capture_suggestion_text(field);
        Check(captured.prefix == item.first && std::string_view(lv_textarea_get_text(field)) == item.first,
              "Preparing a canonical query modified the authored external field");
        std::array<char, 3> shortBuffer{}; std::string_view unavailable;
        Check(!Unicode::FoldUtf8(item.first, shortBuffer.data(), shortBuffer.size(), unavailable, true) && unavailable.empty(),
              "An expanded Indic prefix overflowed its fixed output buffer");
        std::cout << "Indic NFC query probe legacy=" << raw << " normalized=" << normalized << " canonical=" << canonical << '\n';
    }
    std::array<char, 256> previousStorage{}, olderStorage{}; std::string_view previous, older;
    Check(Unicode::FoldUtf8(u8"\u0958लम", previousStorage.data(), previousStorage.size(), previous, true) &&
          Unicode::FoldUtf8("start", olderStorage.data(), olderStorage.size(), older, true), "Canonical Indic context did not fold");
    std::array<lv_advanced_keyboard_suggestion_t, 3> contextual{};
    const auto found = lv_advanced_keyboard_predictor_suggest_folded(predictor, nullptr, 0, previous.data(), previous.size(), older.data(), older.size(), contextual.data(), contextual.size());
    Check(found && std::string_view(contextual[0].text, contextual[0].length) == u8"\u09a1\u09bcাল",
          "Canonicalized Indic previous/older tokens did not select the real trigram candidate");
    lv_advanced_keyboard_predictor_destroy(predictor);
}
}
void ListedQueries(lv_obj_t* field) {
    std::array<char, 256> output{}; std::string_view prepared;
    struct Case { const char* language; const char* input; bool complete; const char* expected; };
    for(const auto& item : std::array<Case,8>{{
        {"de-DE",u8"STRAẞE",false,u8"straße"},{"el-GR",u8"κόσμος",false,u8"κόσμος"},
        {"el-GR",u8"ΚΌΣΜΟΣ",true,u8"κόσμος"},{"el-GR",u8"ΟΣ",false,u8"οσ"},
        {"el-GR",u8"ΟΣ",true,u8"ος"},{"el-GR",u8"Σ",true,u8"σ"},
        {"fr-FR",u8"L’HEURE",false,"l'heure"},{"en-US",u8"Straße",false,"strasse"},
    }}) {
        Check(Unicode::PrepareKeyboardQuery(item.input,output.data(),output.size(),prepared,item.language,item.complete) && prepared==item.expected,
              "Model query changed listed spelling or legacy folding");
    }
    Check(Unicode::FoldUtf8(u8"κόσμος",output.data(),output.size(),prepared) && prepared==u8"κόσμοσ", "Legacy Unicode case fold changed");
    std::array<char,1> shortBuffer{};
    Check(!Unicode::PrepareKeyboardQuery(u8"ß",shortBuffer.data(),shortBuffer.size(),prepared,"de-DE",false) && prepared.empty(), "Listed query overflowed a fixed buffer");
    Check(!Unicode::PrepareKeyboardQuery(std::string_view("\xce",1),output.data(),output.size(),prepared,"el-GR",true) && prepared.empty(), "Partial Greek UTF-8 query was accepted");
    Set(field,u8"STRAẞ"); const auto input=Text::capture_suggestion_text(field);
    Check(Unicode::PrepareKeyboardQuery(input.prefix,output.data(),output.size(),prepared,"de-DE",false),"Cannot prepare sharp-S display prefix");
    std::array<char,256> display{};const auto size=Text::display_suggestion(input,u8"straße",display.data(),display.size(),prepared);
    Check(std::string_view(display.data(),size)==u8"STRAẞE" && std::string_view(lv_textarea_get_text(field))==u8"STRAẞ", "Model-normalized display changed typed prefix or lost German casing");
}
int main() {
    try {
        lv_init(); auto* display = lv_display_create(320, 240); Check(display, "Cannot create a helper-test display");
        auto* field = lv_textarea_create(lv_screen_active());
        Context(field); Scripts(field); Guards(field); ListedQueries(field);
        NfcProbe(field);
        lv_obj_delete(field); lv_display_delete(display);
        std::cout << "Unicode/Han context, caret, casing, acceptance and privacy guards passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
