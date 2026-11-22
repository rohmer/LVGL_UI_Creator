#include "FigmaEditor.h"
#include "../Editor/EditorInputTest.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include <FigmaImporter.h>
#include <Serialization/Base64.h>
#include <Serialization/ImageAsset.h>
#include <Serialization/ProjectScreens.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
namespace Input = EditorInputTest;
void PumpUntil(SdlSimulator& simulator, const std::function<bool()>& ready, const char* error) {
    const auto deadline = SDL_GetTicks() + 20000;
    while (!ready() && SDL_GetTicks() < deadline) { SDL_Delay(10); Input::Require(simulator.ProcessEvents(), "SDL stopped during Figma import"); }
    Input::Require(ready(), error);
}
lv_obj_t* FindFigmaButton(lv_obj_t* window, const char* label) {
    auto* text = Input::FindLabel(window, label);
    Input::Require(text != nullptr, std::string("Figma button missing: ") + label);
    auto* button = Input::ParentButton(text);
    Input::Require(button != nullptr, std::string("Figma button has no action: ") + label);
    return button;
}
bool HasObject(const json& document, const std::string& name) {
    const auto matches = [&](const json& objects) {
        for (const auto& object : objects) if (object.at("base").value("name", std::string{}) == name) return true;
        return false;
    };
    if (document.contains("screens")) {
        for (const auto& screen : document["screens"]) if (matches(screen.at("obj"))) return true;
        return false;
    }
    return matches(document.at("obj"));
}
std::string Bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary); Input::Require(bool(input), "Cannot read Figma fixture asset");
    return {std::istreambuf_iterator<char>(input), {}};
}
std::string ObjectImageId(const json& object) {
    const auto& asset = object.at("img").at("asset");
    if (object.at("base").contains("imageId")) {
        const auto id = object.at("base").at("imageId").get<std::string>();
        if (asset.contains("projectImage")) Input::Require(asset.at("projectImage") == id, "Figma image alias differs from its catalog reference");
        return id;
    }
    return asset.at("projectImage").get<std::string>();
}
std::string ImageId(const json& document, const std::string& name) {
    for (const auto& screen : document.at("screens")) for (const auto& object : screen.at("obj")) {
        if (object.at("base").value("name", std::string{}) != name) continue;
        return ObjectImageId(object);
    }
    throw std::runtime_error("Referenced image widget is missing: " + name);
}
}
void CheckFigmaEditor(UI& editor, SdlSimulator& simulator, const std::filesystem::path& source) {
    auto initial = editor.ProjectDocument();
    initial["obj"].push_back({{"oid", 0}, {"par", -1}, {"text", "Keep this label"},
        {"base", {{"name", "Existing label"}, {"coords", {{"x", 8}, {"y", 8}, {"width", 160}, {"height", 30}}}}}});
    Serialization::ProjectScreens::SyncAlias(initial);
    editor.ApplyDocument(initial);
    const auto baseline = editor.ProjectDocument();
    editor.MenuAction("menu:File");
    Input::Require(Input::FindLabel(lv_layer_top(), "Import Figma design..."), "File menu does not offer Figma import");
    editor.MenuAction("import-figma"); Input::Drain(simulator);
    auto* window = Input::WindowFromTitle("Import Figma design"); Input::Require(window, "Figma import window did not open");
    auto* sourceChoice = Input::FindField(window, "Source", &lv_dropdown_class);
    Input::Require(sourceChoice && lv_dropdown_get_option_count(sourceChoice) == 2, "Figma local and online source options are missing");
    auto* link = Input::FindPlaceholder(window, "https://www.figma.com/design/...");
    auto* token = Input::FindPlaceholder(window, "Figma personal access token");
    Input::Require(link && token, "Figma online link and token fields are missing");
    lv_dropdown_set_selected(sourceChoice, 1); lv_obj_send_event(sourceChoice, LV_EVENT_VALUE_CHANGED, nullptr); Input::Drain(simulator);
    Input::Require(lv_textarea_get_password_mode(token) && lv_textarea_get_password_show_time(token) == 0,
        "Figma token field exposes entered credentials");
    const std::string privateToken = "figd_editor_test_secret";
    Input::Replace(token, privateToken, simulator); Input::AssertFocused(token, simulator);
    Input::Replace(link, "https://figma.com.evil.example/design/AbC123xyz/Screen", simulator); Input::AssertFocused(link, simulator);
    Input::Click(FindFigmaButton(window, "Load from Figma"), simulator); Input::Drain(simulator);
    auto* import = FindFigmaButton(window, "Import design");
    Input::Require(lv_obj_is_disabled(import), "An invalid online Figma link left import enabled");
    Input::Require(editor.ProjectDocument() == baseline && editor.ProjectDocument().dump().find(privateToken) == std::string::npos,
        "Online credentials or failed link validation changed the project");
    Input::Replace(link, "https://www.figma.com/design/AbC123xyz/Screen", simulator);
    Input::Replace(token, "", simulator);
    Input::Click(FindFigmaButton(window, "Load from Figma"), simulator); Input::Drain(simulator);
    Input::Require(lv_obj_is_disabled(import) && editor.ProjectDocument() == baseline,
        "A missing online token started or applied an import");
    lv_dropdown_set_selected(sourceChoice, 0); lv_obj_send_event(sourceChoice, LV_EVENT_VALUE_CHANGED, nullptr); Input::Drain(simulator);
    std::vector<lv_obj_t*> fields = {Input::FindPlaceholder(window, ".figma.json or .json"), Input::FindField(window, "Scale (%)")};
    Input::Require(fields[0] && fields[1], "Figma file and scale fields are missing");
    Input::Replace(fields[0], source.u8string(), simulator);
    Input::AssertFocused(fields[0], simulator);
    Input::Require(lv_obj_is_disabled(import), "Figma import was enabled before reviewing a design");
    Input::Click(FindFigmaButton(window, "Load design"), simulator);
    PumpUntil(simulator, [&] { return !lv_obj_is_disabled(import); }, "Figma design did not finish loading and reviewing");
    Input::Require(editor.ProjectDocument() == baseline, "Reviewing a Figma file modified the current project");
    std::vector<lv_obj_t*> choices = {Input::FindField(window, "Frames", &lv_dropdown_class), Input::FindField(window, "Destination", &lv_dropdown_class)};
    Input::Require(choices[0] && choices[1], "Figma frame and destination selectors are missing");
    Input::Require(lv_dropdown_get_option_count(choices[0]) == 3, "Figma frame selector did not list both frames");
    Input::Click(import, simulator); Input::Drain(simulator);
    const auto appended = editor.ProjectDocument();
    Input::Require(Serialization::ProjectScreens::Count(appended) == 3, "Figma default import did not append both frames as screens");
    Input::Require(HasObject(appended, "Existing label") && HasObject(appended, "Heading") && HasObject(appended, "About caption"), "Figma import lost existing or imported editable widgets");
    Input::Require(editor.Undo() && editor.ProjectDocument() == baseline, "Figma append was not a single undo transaction");
    Input::Require(editor.Redo() && editor.ProjectDocument() == appended, "Figma redo did not restore the imported screens");

    // Change the frame and scale using normal controls; the previous review must
    // be invalidated until the new options have been converted successfully.
    lv_dropdown_set_selected(choices[0], 2); lv_obj_send_event(choices[0], LV_EVENT_VALUE_CHANGED, nullptr);
    lv_dropdown_set_selected(choices[1], 1); lv_obj_send_event(choices[1], LV_EVENT_VALUE_CHANGED, nullptr);
    Input::Replace(fields[1], "50", simulator); Input::AssertFocused(fields[1], simulator);
    Input::Require(lv_obj_is_disabled(import), "Changing Figma options retained a stale conversion plan");
    Input::Click(FindFigmaButton(window, "Review conversion"), simulator);
    PumpUntil(simulator, [&] { return !lv_obj_is_disabled(import); }, "Selected frame conversion did not finish");
    Input::Click(import, simulator); Input::Drain(simulator);
    const auto replaced = editor.ProjectDocument();
    Input::Require(Serialization::ProjectScreens::Count(replaced) == 1, "Figma replace imported unselected frames");
    Input::Require(replaced.at("canvas").at("width") == 160 && replaced.at("canvas").at("height") == 120, "Figma scale option did not resize the imported screen");
    Input::Require(!HasObject(replaced, "Existing label") && HasObject(replaced, "About caption"), "Figma replace preserved the wrong widgets");
    Input::Require(editor.Undo() && editor.ProjectDocument() == appended, "Figma replace undo lost prior screens");
    Input::Require(editor.Redo() && editor.ProjectDocument() == replaced, "Figma replace redo lost selected screen");

    Input::Replace(fields[0], (source.parent_path() / "missing-design.figma.json").u8string(), simulator);
    auto* load = FindFigmaButton(window, "Load design"); Input::Click(load, simulator);
    PumpUntil(simulator, [&] { return !lv_obj_is_disabled(load); }, "Failed Figma load did not restore its controls");
    Input::Require(lv_obj_is_disabled(import), "Failed Figma load left a stale import enabled");
    Input::Require(editor.ProjectDocument() == replaced, "A failed Figma file load changed the current project");

    // Real appends must rename resource IDs on collision and update all image
    // references without altering the project's existing variable bindings.
    auto bundle = json::parse(Bytes(source));
    auto& home = bundle["document"]["children"][0]["children"][0];
    home["children"].push_back({{"id", "5:1"}, {"name", "Raster logo"}, {"type", "VECTOR"},
        {"absoluteBoundingBox", {{"x", 140}, {"y", 320}, {"width", 32}, {"height", 24}}}});
    const auto fixtureImages = source.parent_path().parent_path().parent_path().parent_path() / "Fixtures/Images";
    const auto png = Bytes(fixtureImages / "sample.png");
    const auto jpeg = Bytes(fixtureImages / "sample.jpg");
    bundle["uicreatorFigmaVersion"] = 1;
    bundle["uicreatorAssets"] = {{"5:1", {{"name", "Logo.png"}, {"mimeType", "image/png"},
        {"data", base64_encode(reinterpret_cast<const unsigned char*>(png.data()), static_cast<unsigned int>(png.size()))}}}};
    const auto planned = FigmaImport::Convert(bundle);
    const auto collision = planned.project.at("images")[0].at("id").get<std::string>();
    auto withResources = editor.ProjectDocument();
    withResources["images"] = {{{"id", collision}, {"name", planned.project.at("images")[0].at("name")}, {"image", {{"name", "Existing photograph.jpg"},
        {"sourceData", base64_encode(reinterpret_cast<const unsigned char*>(jpeg.data()), static_cast<unsigned int>(jpeg.size()))}}}}};
    const auto jpegDecoded = Serialization::ImageAsset::Decode(withResources["images"][0]["image"]);
    withResources["images"][0]["image"]["width"] = jpegDecoded.width;
    withResources["images"][0]["image"]["height"] = jpegDecoded.height;
    withResources["images"][0]["image"]["frames"] = jpegDecoded.delays.size();
    int next = 0;
    for (const auto& object : withResources.at("obj")) next = std::max(next, object.at("oid").get<int>() + 1);
    withResources["obj"].push_back({{"oid", next}, {"par", -1}, {"img", {{"asset", {{"projectImage", collision}}}}},
        {"base", {{"name", "Existing image"}, {"imageId", collision}, {"coords", {{"x", 8}, {"y", 70}, {"width", 32}, {"height", 24}}}}}});
    withResources["variables"] = {{{"var", {{"name", "retained_count"}, {"dataType", 1}, {"default", 7}, {"history", 2}, {"value", {3, 7}},
        {"bindings", {{{"screen", 0}, {"oid", 1}, {"property", "/text"}, {"direction", "to-widget"}}}}}}}};
    Serialization::ProjectScreens::SyncAlias(withResources); editor.ApplyDocument(withResources);
    const auto resourcesBaseline = editor.ProjectDocument();
    struct Temporary {
        std::filesystem::path directory, file;
        ~Temporary() { std::error_code error; std::filesystem::remove(file, error); std::filesystem::remove(directory, error); }
    } temporary;
    temporary.directory = std::filesystem::temp_directory_path() / ("uicreator-figma-import-" + std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directories(temporary.directory); temporary.file = temporary.directory / "bundle.figma.json";
    { std::ofstream output(temporary.file, std::ios::binary); output << bundle.dump(); Input::Require(bool(output), "Cannot write bundled Figma fixture"); }
    Input::Replace(fields[0], temporary.file.u8string(), simulator);
    lv_dropdown_set_selected(choices[1], 0); lv_obj_send_event(choices[1], LV_EVENT_VALUE_CHANGED, nullptr);
    Input::Click(load, simulator);
    PumpUntil(simulator, [&] { return !lv_obj_is_disabled(import); }, "Bundled Figma file did not finish reviewing");
    Input::Click(import, simulator); Input::Drain(simulator);
    const auto merged = editor.ProjectDocument();
    Input::Require(merged.at("images").size() == 2, "Appending a Figma bundle lost an existing or imported image");
    Input::Require(merged.at("images")[0].at("name") != merged.at("images")[1].at("name"), "Figma image name collision was not resolved");
    Input::Require(ImageId(merged, "Existing image") == collision, "Figma append changed the existing image reference");
    Input::Require(ImageId(merged, "Raster logo") != collision, "Figma image collision was not renamed in the imported widget");
    Input::Require(merged.at("variables") == resourcesBaseline.at("variables"), "Figma append changed existing variable bindings or history");
    Input::Require(editor.Undo() && editor.ProjectDocument() == resourcesBaseline, "Resource append undo did not restore the old catalog");
    Input::Require(editor.Redo() && editor.ProjectDocument() == merged, "Resource append redo did not restore both image references");
    Input::Click(FindFigmaButton(window, "Review conversion"), simulator);
    PumpUntil(simulator, [&] { return !lv_obj_is_disabled(import); }, "Reviewing the same Figma bundle a second time did not finish");
    Input::Click(import, simulator); Input::Drain(simulator);
    const auto repeated = editor.ProjectDocument();
    Input::Require(repeated.at("images").size() == 2, "Repeated Figma import duplicated an identical image payload");
    Input::Require(Serialization::ProjectScreens::Count(repeated) == Serialization::ProjectScreens::Count(merged) + 2,
        "Repeated Figma import did not append both frames");
    std::set<std::string> resourceIds, resourceNames;
    std::map<std::string, json> resources;
    for (const auto& image : repeated.at("images")) {
        const auto id = image.at("id").get<std::string>();
        Input::Require(resourceIds.insert(id).second, "Repeated Figma import duplicated an image ID");
        Input::Require(resourceNames.insert(image.at("name").get<std::string>()).second, "Repeated Figma import duplicated an image name");
        resources[id] = image.at("image");
    }
    size_t rasterCount = 0;
    for (const auto& screen : repeated.at("screens")) for (const auto& object : screen.at("obj")) {
        if (object.at("base").value("name", std::string{}) != "Raster logo") continue;
        const auto id = ObjectImageId(object);
        Input::Require(resources.count(id) && resources.at(id).at("sourceData") == planned.project.at("images")[0].at("image").at("sourceData"),
            "Repeated Figma import bound a raster widget to the wrong image");
        ++rasterCount;
    }
    Input::Require(rasterCount == 2 && repeated.at("variables") == resourcesBaseline.at("variables"), "Repeated Figma import lost raster widgets or variable bindings");
    Input::Require(editor.Undo() && editor.ProjectDocument() == merged, "Repeated Figma import was not one undo transaction");
    Input::Click(EditorWindow::CloseButton(window), simulator); Input::Drain(simulator);
    Input::Require(!Input::WindowFromTitle("Import Figma design"), "Figma close left its window active");
    std::cout << "Figma menu, local/online physical input, masked credentials, URL validation, review, append/replace, frame/scale selection, resource collisions, bindings, undo/redo and failed-load checks passed\n";
}
