#pragma once
#include "AdvancedKeyboardSettings.h"
#include <algorithm>
#include <set>

namespace Serialization {
// Project options are authoring metadata. Execution/export creates ordinary
// keyboard records, one per screen, for the existing portable widget loader.
class ProjectKeyboard {
public:
    using json = nlohmann::json;
    static json Normalize(const json& input)
    {
        if (!input.is_object()) throw std::invalid_argument("Project keyboard settings must be an object");
        if (input.contains("enabled") && !input.at("enabled").is_boolean())
            throw std::invalid_argument("Project keyboard enabled must be a boolean");
        auto result = NormalizeAdvancedKeyboardSettings(input);
        result["enabled"] = input.value("enabled", false);
        result["targetOid"] = -1;
        result["autoFocus"] = true;
        result["hideOnDefocus"] = false;
        result.erase("screenManaged");
        if (result.contains("height")) {
            const auto& height = result.at("height");
            if (height.is_null()) result.erase("height");
            else if (!height.is_number_integer() || height.get<int64_t>() < 1 || height.get<int64_t>() > 32767)
                throw std::invalid_argument("Project keyboard height must be between 1 and 32767");
        }
        return result;
    }
    static int DefaultHeight(int screenHeight)
    { return (std::min)(240, (std::max)(1, screenHeight * 3 / 5)); }

    static bool Managed(const json& record)
    {
        return record.is_object() && record.contains("advancedKeyboard") &&
            record.at("advancedKeyboard").is_object() && record.at("advancedKeyboard").value("screenManaged", false);
    }
    static void Materialize(json& document)
    {
        if (!document.is_object() || !document.contains("advancedKeyboard")) return;
        const auto settings = Normalize(document.at("advancedKeyboard"));
        document.erase("advancedKeyboard");
        if (document.contains("screens") && document["screens"].is_array())
            for (auto& screen : document["screens"]) MaterializeScreen(screen, settings);
        if (document.contains("obj")) MaterializeScreen(document, settings);
        // Authoring retains its model library. A device/run copy keeps only
        // models used by actual widgets; do not decode or copy model payloads.
        if (document.contains("tries") && document["tries"].is_array()) {
            std::set<std::string> used;
            auto collect = [&](const auto& self, const json& value) -> void {
                if (value.is_array()) { for (const auto& child : value) self(self,child); return; }
                if (!value.is_object()) return;
                if (value.contains("base") && value.contains("advancedKeyboard")) {
                    const auto& keyboard = value.at("advancedKeyboard");
                    const auto language = keyboard.value("language", std::string("en-US"));
                    used.insert(keyboard.value("trieId",language == "en-US" ? std::string("builtin-en-us") : std::string{}));
                }
                for (const auto& item : value.items())
                    if (item.key()!="tries" && item.key()!="components" && item.key()!="componentLibrary" && item.key()!="design") self(self,item.value());
            };
            collect(collect,document);
            auto& tries = document["tries"];
            tries.erase(std::remove_if(tries.begin(),tries.end(),[&](const json& asset) {
                return asset.is_object() && asset.contains("id") && asset["id"].is_string() && !used.count(asset["id"].get<std::string>());
            }),tries.end());
            if (tries.empty()) document.erase("tries");
        }
    }
private:
    static void MaterializeScreen(json& document, const json& settings)
    {
        if (!document.contains("obj")) document["obj"] = json::array();
        auto& records = document["obj"];
        if (!records.is_array()) throw std::invalid_argument("Screen widget records must be an array");
        json previous = json::object();
        for (const auto& record : records) if (record.contains("advancedKeyboard")) { previous = record; break; }
        // Explicit project options supersede earlier authored keyboards on the
        // execution copy. Projects without project options keep legacy behavior.
        records.erase(std::remove_if(records.begin(), records.end(), [](const json& record) {
            return record.contains("advancedKeyboard");
        }), records.end());
        if (!settings.at("enabled").get<bool>()) return;
        std::set<int> ids;
        for (const auto& record : records) if (record.contains("oid")) ids.insert(record.at("oid").get<int>());
        int id = previous.value("oid", 0);
        while (ids.count(id)) { if (id == INT32_MAX) throw std::invalid_argument("No free keyboard object ID"); ++id; }
        const auto canvas = document.value("canvas", json::object());
        const int width = canvas.value("width", 320), screenHeight = canvas.value("height", 240);
        if (width < 1 || width > 32767 || screenHeight < 1 || screenHeight > 32767)
            throw std::invalid_argument("Invalid screen dimensions for keyboard");
        const int height = (std::min)(settings.value("height", DefaultHeight(screenHeight)), screenHeight);
        auto properties = settings;
        properties.erase("enabled"); properties.erase("height"); properties["screenManaged"] = true;
        auto base = previous.value("base", json::object());
        base["name"] = "Screen Advanced Keyboard"; base["hidden"] = false;
        base["coords"] = {{"x",0},{"y",screenHeight-height},{"width",width},{"height",height}};
        if (!base.contains("flags") || !base["flags"].is_object()) base["flags"] = json::object();
        base["flags"]["hidden"] = false; base["flags"]["scrollable"] = false;
        records.push_back({{"oid",id},{"par",-1},{"base",std::move(base)},{"advancedKeyboard",std::move(properties)}});
    }
};
}
