#include "../../../3rdParty/LVGL/lvgl/include/lvgl/widgets/lv_bootstrap.h"
#include <Serialization/Project.h>
#include <Serialization/ObjectSerializer.h>
#include "../../../CodeGeneration/CCodeGenerator.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Entries = std::vector<std::pair<lv_obj_t*, std::string>>;
void Require(bool condition, const std::string& message) { if(!condition) throw std::runtime_error(message); }
void DeleteRoots(const std::vector<lv_obj_t*>& objects) {
    // Snapshot before deletion: nested object pointers cease to be valid afterwards.
    std::vector<lv_obj_t*> roots;
    for(auto* object : objects) if(lv_obj_get_parent(object) == lv_screen_active()) roots.push_back(object);
    for(auto* object : roots) lv_obj_delete(object);
}
void CheckActions(const std::vector<lv_obj_t*>& objects) {
    for(auto* object : objects) {
        const auto record = Serialization::ObjectSerializer::SerializeObject(object);
        const auto action = record["base"].value("bootstrapAction", std::string{});
        if(action.empty()) continue;
        auto* target = action == "dismiss" ? lv_obj_get_parent(object) : lv_obj_get_child(lv_obj_get_parent(object), 1);
        Require(target != nullptr, "Action target missing");
        const bool hidden = lv_obj_is_hidden(target);
        lv_obj_send_event(object, LV_EVENT_CLICKED, nullptr);
        Require(lv_obj_is_hidden(target) == (action == "dismiss" || !hidden), "Action did not change visibility");
        lv_obj_set_hidden(target, hidden);
    }
}
}

int main(int argc, char** argv)
{
    try {
        Require(argc == 2, "Expected generated-output directory");
        lv_init();
        Require(lv_display_create(1024, 768) != nullptr, "Display allocation failed");
        for(int preset = 0; preset < LV_BOOTSTRAP_PRESET_COUNT; ++preset) {
            Entries entries;
            auto* root = lv_bootstrap_create(lv_screen_active(), static_cast<lv_bootstrap_preset_t>(preset), preset % 2 != 0,
                [](lv_obj_t* object, const char* name, void* context) {static_cast<Entries*>(context)->emplace_back(object, name);}, &entries);
            Require(root != nullptr && !entries.empty(), "Preset creation failed");
            std::vector<lv_obj_t*> objects;
            for(const auto& entry : entries) {
                json base = {{"name", entry.second}};
                if(const char* action = lv_bootstrap_get_action(entry.first)) base["bootstrapAction"] = action;
                // Existing recipe callbacks are already bound; base metadata is written
                // after creation to avoid binding the same behavior a second time.
                Serialization::LVObject::FromJSON({{"name", entry.second}}, entry.first);
                static_cast<ObjectUserData*>(lv_obj_get_user_data(entry.first))->objectJson["base"] = base;
                objects.push_back(entry.first);
            }
            CheckActions(objects);
            auto document = Serialization::Project::ToJSON(objects, lv_screen_active(), json::array(), 1024, 768);
            const auto originalCount = objects.size();
            CCodeGenerator generator;
            Require(generator.LoadProject(document), generator.GetLastError());
            json summary;
            Require(generator.Preflight(summary), generator.GetLastError());
            Require(summary.at("files").get<size_t>() == 8 && summary.at("sourceBytes").get<size_t>() > 0,
                "Preflight returned an invalid output summary");
            const auto directory = std::filesystem::path(argv[1]) / std::to_string(preset);
            Require(generator.ExportProject(directory.string()), std::string(lv_bootstrap_preset_name(static_cast<lv_bootstrap_preset_t>(preset))) + ": " + generator.GetLastError());
            size_t actualBytes = 0;
            for(const char* name : {"ui.h", "ui.c", "ui_assets.h", "ui_assets.c", "ui_requirements.h", "CMakeLists.txt", "README.md", "manifest.json"})
                actualBytes += static_cast<size_t>(std::filesystem::file_size(directory / name));
            Require(actualBytes == summary.at("sourceBytes").get<size_t>(), "Preflight source-byte estimate differs from actual output");
            Require(std::filesystem::file_size(directory / "ui_assets.c") == summary.at("assetSourceBytes").get<size_t>(), "Preflight asset source differs from actual output");
            const auto binary = Serialization::Protobuf::Encode(Serialization::EntityKind::Project, "project", document);
            { std::ofstream stream(directory / "fixture.json"); stream << document.dump(2); Require(bool(stream), "Cannot write JSON fixture"); }
            { std::ofstream stream(directory / "fixture.pb", std::ios::binary); stream.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size())); Require(bool(stream), "Cannot write Protobuf fixture"); }
            lv_obj_delete(root);
            auto restored = Serialization::Project::FromJSON(document);
            Require(restored.objects.size() == originalCount, "JSON round trip lost document objects");
            CheckActions(restored.objects);
            DeleteRoots(restored.objects);
            restored = Serialization::Project::FromProtobuf(binary);
            Require(restored.objects.size() == originalCount, "Protobuf round trip lost document objects");
            CheckActions(restored.objects);
            DeleteRoots(restored.objects);
        }
        Require(lv_bootstrap_create(lv_screen_active(), static_cast<lv_bootstrap_preset_t>(LV_BOOTSTRAP_PRESET_COUNT), false, nullptr, nullptr) == nullptr,
            "Invalid preset was accepted");
        lv_deinit();
        std::cout << "Verified 36 component recipes and 13 template recipes, JSON/Protobuf recovery, actions and C export.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
