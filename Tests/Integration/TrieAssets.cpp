#include <Serialization/ProjectTries.h>
#include <Serialization/AdvancedKeyboard.h>
#include <lv_advanced_keyboard_prediction.h>
#include "../../Runtime/include/lvgl_runtime.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace Serialization;
namespace {
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
const uint8_t kModel[] = {'N','G','T','R',3,0,0,0,4,0,0,0,0,0,0,0,0,2,0,2,'a',1,1,'a',1,1};
void Predict(lv_obj_t* keyboard)
{
    auto* predictor = lv_advanced_keyboard_get_predictor(keyboard);
    Check(predictor && lv_advanced_keyboard_predictor_node_count(predictor) == 4, "Custom trie predictor missing");
    lv_advanced_keyboard_suggestion_t words[3]{};
    const auto count = lv_advanced_keyboard_predictor_suggest_folded(predictor, "a", 1, nullptr, 0, nullptr, 0, words, 3);
    Check(count == 1 && words[0].length == 1 && words[0].text[0] == 'a', "Custom model prediction changed");
}
}
int main()
{
    try {
        std::string id, sameId;
        auto tries = ProjectTries::Import(nlohmann::json::array(), "C:\\private\\sample.ngtr", kModel, sizeof kModel, &id);
        Check(tries.size() == 1 && tries[0]["name"] == "sample.ngtr", "Import retained an external path");
        Check(tries[0].contains("indexData"), "Import did not retain its generated index");
        auto duplicate = ProjectTries::Import(tries, "Other name.ngtr", kModel, sizeof kModel, &sameId);
        Check(duplicate == tries && sameId == id, "Import did not deduplicate byte-identical models");
        nlohmann::json document = {{"formatVersion", 2}, {"lvglVersion", "9.6.0"}, {"runtimeTarget", "9.6"},
            {"canvas", {{"width", 640}, {"height", 400}}}, {"tries", tries},
            {"extra", nlohmann::json::binary({0,1,255})},
            {"obj", {{{"oid", 7}, {"par", -1}, {"base", nlohmann::json::object()},
                       {"advancedKeyboard", {{"trieId", id}, {"targetOid", -1}, {"autoFocus", false}}}}}}};
        const auto bytes = Protobuf::Encode(EntityKind::Project, "project", document);
        auto owned = Protobuf::Decode(bytes);
        Check(owned.data["tries"][0]["data"].is_binary() && owned.data["tries"][0]["indexData"].is_binary(), "Wire trie payloads were not binary");
        Check(ProjectTries::Normalize(owned.data.at("tries")) == tries, "Binary catalog did not round-trip to canonical authoring data");
        auto borrowed = Protobuf::DecodeBorrowedAssets(bytes.data(), bytes.size());
        Check(borrowed.borrowedAssets.size() == 2 && borrowed.data["extra"].is_binary(), "Borrow mode captured unrelated binary data");
        for (const auto& item : borrowed.borrowedAssets) {
            Check(item.second.data >= bytes.data() && item.second.size <= bytes.size() &&
                  size_t(item.second.data - bytes.data()) <= bytes.size() - item.second.size,
                  "Borrowed trie bytes were copied or escaped the input range");
        }
        auto forged = borrowed.data;
        bool rejected = false;
        try { ProjectTries::Load(forged); } catch (const std::exception&) { rejected = true; }
        Check(rejected, "Borrowed markers without their trusted span table were accepted");
        lv_init(); auto* display = lv_display_create(640, 400);
        Check(display, "Cannot create display");
        auto* previous = lv_screen_active();
        {
            auto catalog = ProjectTries::Load(borrowed.data, borrowed.borrowedAssets);
            Check(!borrowed.data.contains("tries"), "Bulk catalog stayed in screen metadata");
            auto* keyboard = lv_advanced_keyboard_create(previous); Check(keyboard, "Cannot create keyboard");
            { ProjectTries::Scope scope(catalog); ProjectTries::Attach(keyboard, id, true, 0.3); }
            catalog.reset(); Predict(keyboard);
            ProjectTries::Attach(keyboard, id, false, 0.4); Predict(keyboard);
            lv_obj_delete(keyboard);
        }
        {
            auto temporary = bytes;
            auto decoded = Protobuf::DecodeBorrowedAssets(temporary.data(), temporary.size());
            auto catalog = ProjectTries::LoadOwned(decoded.data, decoded.borrowedAssets);
            std::fill(temporary.begin(), temporary.end(), 0); temporary.clear(); temporary.shrink_to_fit();
            auto* keyboard = lv_advanced_keyboard_create(previous); Check(keyboard, "Cannot create owned trie keyboard");
            { ProjectTries::Scope scope(catalog); ProjectTries::Attach(keyboard, id); }
            catalog.reset(); Predict(keyboard); lv_obj_delete(keyboard);
        }
        for (int cycle = 0; cycle < 3; ++cycle) {
            auto temporary = bytes; lvgl_runtime_t* runtime = nullptr; char error[512]{};
            Check(lvgl_runtime_load_memory(temporary.data(), temporary.size(), nullptr, &runtime, error, sizeof error), error);
            std::fill(temporary.begin(), temporary.end(), 0); temporary.clear(); temporary.shrink_to_fit();
            Predict(lvgl_runtime_object(runtime, 0, 7));
            lvgl_runtime_destroy(runtime); Check(lv_screen_active() == previous, "Owned runtime did not restore the previous screen");
            Check(lvgl_runtime_load_memory_borrowed_assets(bytes.data(), bytes.size(), nullptr, &runtime, error, sizeof error), error);
            Predict(lvgl_runtime_object(runtime, 0, 7));
            lvgl_runtime_destroy(runtime); Check(lv_screen_active() == previous, "Borrowed runtime did not restore the previous screen");
        }
        lv_display_delete(display); lv_deinit();
        std::cout << "Trie asset import/dedup, binary wire data, trusted flash borrowing, retained lifetimes and owned-input release passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
