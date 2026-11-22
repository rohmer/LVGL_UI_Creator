#include <FigmaImporter.h>
#include <CCodeGenerator.h>
#include <MicroPythonCodeGenerator.h>
#include <XmlCodeGenerator.h>
#include <Serialization/Base64.h>
#include <Serialization/Project.h>
#include <Serialization/ProjectScreens.h>
#include <Serialization/RuntimeFeatures.h>
#include <Serialization/Xml.h>
#include <lvgl_runtime.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(value) do { if (!(value)) throw std::runtime_error(std::string("Line ") + std::to_string(__LINE__) + ": " #value); } while (0)
namespace {
using json = nlohmann::json;
namespace fs = std::filesystem;
using Runtime = std::unique_ptr<lvgl_runtime_t, decltype(&lvgl_runtime_destroy)>;

std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary); CHECK(file);
    return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary); CHECK(file);
    file.write(text.data(), static_cast<std::streamsize>(text.size())); CHECK(file);
}
json Source() { return json::parse(Read(fs::u8path(UICREATOR_FIGMA_SOURCE_ROOT) / "Tests/Integration/Figma/fixtures/native.figma.json")); }
const json& Named(const json& objects, const std::string& name) {
    for (const auto& object : objects)
        if (object.at("base").value("name", std::string{}) == name) return object;
    throw std::runtime_error("Imported object is missing: " + name);
}
bool Contains(const json& objects, const std::string& name) {
    return std::any_of(objects.begin(), objects.end(), [&](const json& record) {
        return record.at("base").value("name", std::string{}) == name;
    });
}
json Styles(const json& object) {
    for (const auto& style : object.at("base").at("nativeStyles"))
        if (style.value("selector", 0) == 0) return style.at("values");
    throw std::runtime_error("Main native styles are missing");
}
template<class Action> void Reject(Action action) {
    bool rejected = false; try { action(); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
}
void CheckHierarchy(const json& objects) {
    std::set<int> seen;
    for (const auto& object : objects) {
        const auto id = object.at("oid").get<int>();
        CHECK(seen.insert(id).second);
        const auto parent = object.at("par").get<int>();
        CHECK(parent == -1 || seen.count(parent));
        const auto& coords = object.at("base").at("coords");
        CHECK(coords.at("width").get<int>() >= 1 && coords.at("height").get<int>() >= 1);
    }
}
void Mapping() {
    const auto source = Source();
    const auto frames = FigmaImport::Frames(source);
    CHECK(frames.size() == 2 && frames[0].id == "1:1" && frames[0].name == "Home");
    CHECK(frames[0].width == 320 && frames[0].height == 240);
    const auto result = FigmaImport::Convert(source);
    const auto& document = result.project;
    CHECK(document.at("screens").size() == 2);
    CHECK(document.at("screens")[0].at("name") == "Home");
    CHECK(document.at("screens")[1].at("name") == "About");
    CHECK(document.at("screens")[0].at("canvas").at("width") == 320);
    CHECK(document.at("obj") == document.at("screens")[0].at("obj"));
    const auto& objects = document.at("screens")[0].at("obj");
    const auto& root = Named(objects, "Home");
    const auto& card = Named(objects, "Card");
    const auto& background = Named(objects, "Card background");
    const auto& heading = Named(objects, "Heading");
    CHECK(root.at("par") == -1 && root.at("base").at("coords").at("x") == 0);
    CHECK(card.at("par") == root.at("oid"));
    CHECK(card.at("base").at("coords").at("x") == 16 && card.at("base").at("coords").at("y") == 24);
    CHECK(heading.at("par") == card.at("oid"));
    CHECK(heading.at("base").at("coords").at("x") == 12 && heading.at("base").at("coords").at("y") == 12);
    CHECK(heading.at("text") == "Ready & connected — café");
    CHECK(Styles(background).at("radius") == 12);
    CHECK(Styles(background).at("bg_color") == 0xff8000);
    CHECK(std::abs(Styles(background).at("bg_opa").get<int>() - 128) <= 1);
    CHECK(Styles(background).at("border_width") == 2);
    CHECK(Styles(background).at("border_color") == 0xffffff);
    CHECK(Styles(heading).at("text_color") == 0xffffff);
    CHECK(!Contains(objects, "Hidden object"));
    CHECK(!result.warnings.empty()); // Source fonts are approximated by installed LVGL fonts.
    size_t count = 0;
    for (const auto& screen : document.at("screens")) { CheckHierarchy(screen.at("obj")); count += screen.at("obj").size(); }
    CHECK(result.objects == count);

    FigmaImport::Options selected; selected.nodeIds = {"1:2"}; selected.scale = 2;
    const auto enlarged = FigmaImport::Convert(source, selected);
    CHECK(enlarged.project.at("screens").size() == 1);
    CHECK(enlarged.project.at("screens")[0].at("canvas").at("width") == 640);
    const auto& about = Named(enlarged.project.at("obj"), "About caption");
    CHECK(about.at("base").at("coords").at("x") == 40 && about.at("base").at("coords").at("width") == 520);
    FigmaImport::Options hidden; hidden.includeHidden = true;
    CHECK(Contains(FigmaImport::Convert(source, hidden).project.at("obj"), "Hidden object"));
    auto noHiddenBounds = source;
    noHiddenBounds["document"]["children"][0]["children"][0]["children"][0]["children"][3]["absoluteBoundingBox"] = nullptr;
    CHECK(!Contains(FigmaImport::Convert(noHiddenBounds).project.at("obj"), "Hidden object"));
    const auto hiddenResult = FigmaImport::Convert(noHiddenBounds, hidden);
    CHECK(!Contains(hiddenResult.project.at("obj"), "Hidden object") && !hiddenResult.warnings.empty());
}
void Formats() {
    const auto source = Source();
    const auto& home = source.at("document").at("children")[0].at("children")[0];
    const auto& about = source.at("document").at("children")[0].at("children")[1];
    CHECK(FigmaImport::Frames(home).size() == 1);
    CHECK(FigmaImport::Frames(json{{"document", home}}).size() == 1);
    const auto api = json{{"name", "Node response"}, {"nodes", {{"1:1", {{"document", home}}}, {"1:2", {{"document", about}}}}}};
    CHECK(FigmaImport::Frames(api).size() == 2);
    const auto converted = FigmaImport::Convert(api);
    CHECK(converted.project.at("screens").size() == 2);
    CHECK(Named(converted.project.at("screens")[0].at("obj"), "Heading").at("text") == "Ready & connected — café");
    auto bundle = source; bundle["uicreatorFigmaVersion"] = 1; bundle["uicreatorAssets"] = json::object();
    CHECK(FigmaImport::Convert(bundle).project == FigmaImport::Convert(source).project);
}
json RasterSource() {
    auto source = Source();
    auto& home = source["document"]["children"][0]["children"][0];
    home["children"].push_back({{"id", "5:1"}, {"name", "Brand mark"}, {"type", "VECTOR"},
        {"absoluteBoundingBox", {{"x", 140}, {"y", 320}, {"width", 32}, {"height", 24}}}});
    const auto png = Read(fs::u8path(UICREATOR_FIGMA_SOURCE_ROOT) / "Tests/Fixtures/Images/sample.png");
    source["uicreatorFigmaVersion"] = 1;
    source["uicreatorAssets"] = {{"5:1", {{"name", "Brand mark.png"}, {"mimeType", "image/png"},
        {"data", base64_encode(reinterpret_cast<const unsigned char*>(png.data()), static_cast<unsigned int>(png.size()))}}}};
    return source;
}
void Assets() {
    const auto source = RasterSource();
    const auto result = FigmaImport::Convert(source);
    const auto& images = result.project.at("images"); CHECK(images.size() == 1);
    const auto& image = Named(result.project.at("obj"), "Brand mark");
    CHECK(image.contains("img"));
    CHECK(image.at("img").at("innerAlign") == int(LV_IMAGE_ALIGN_STRETCH));
    CHECK(image.at("img").at("asset").at("projectImage") == images[0].at("id"));
    const auto decoded = base64_decode(images[0].at("image").at("sourceData").get<std::string>());
    CHECK(decoded == Read(fs::u8path(UICREATOR_FIGMA_SOURCE_ROOT) / "Tests/Fixtures/Images/sample.png"));
    CHECK(image.at("base").at("coords").at("x") == 40 && image.at("base").at("coords").at("y") == 120);
    auto translucent = source;
    auto& home = translucent["document"]["children"][0]["children"][0]; home["opacity"] = 0.5;
    home["children"].back()["opacity"] = 0.5;
    const auto alpha = FigmaImport::Convert(translucent);
    CHECK(std::abs(Styles(Named(alpha.project.at("obj"), "Home")).at("opa").get<int>() - 128) <= 1);
    CHECK(Styles(Named(alpha.project.at("obj"), "Brand mark")).at("opa") == 255); // The PNG already contains its own opacity.
    auto unsupported = source; unsupported.erase("uicreatorAssets");
    CHECK(!FigmaImport::Convert(unsupported).warnings.empty());
    auto invalid = source; invalid["uicreatorAssets"]["5:1"]["data"] = "definitely not base64!";
    Reject([&] { FigmaImport::Convert(invalid); });
}
void Malformed() {
    Reject([] { FigmaImport::Convert(nullptr); });
    Reject([] { FigmaImport::Convert(json::object()); });
    Reject([] { FigmaImport::Convert(json{{"document", { {"type", "DOCUMENT"}, {"children", json::array()} }}}); });
    const auto source = Source();
    FigmaImport::Options options;
    for (const double scale : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        options.scale = scale; Reject([&] { FigmaImport::Convert(source, options); });
    }
    options = {}; options.nodeIds = {"missing:node"}; Reject([&] { FigmaImport::Convert(source, options); });
    auto invalid = source; invalid["document"]["children"][0]["children"][0]["absoluteBoundingBox"]["width"] = "wide";
    Reject([&] { FigmaImport::Convert(invalid); });
    invalid = source; invalid["document"]["children"][0]["children"][0]["absoluteBoundingBox"]["width"] = 1e100;
    Reject([&] { FigmaImport::Convert(invalid); });
    invalid = source; invalid["document"]["children"][0]["children"][0]["absoluteBoundingBox"]["x"] = std::numeric_limits<double>::infinity();
    Reject([&] { FigmaImport::Convert(invalid); });
    invalid = source; invalid["document"]["children"][0]["children"][0]["children"] = "children are not an array";
    Reject([&] { FigmaImport::Convert(invalid); });
    invalid = source; invalid["document"]["children"][0]["children"][1]["id"] = "1:1";
    Reject([&] { FigmaImport::Convert(invalid); });
    options = {}; options.nodeIds = {"1:1", "1:1"}; Reject([&] { FigmaImport::Convert(source, options); });
    json deep = {{"id", "deep:leaf"}, {"type", "RECTANGLE"}, {"name", "Deep leaf"},
        {"absoluteBoundingBox", {{"x", 0}, {"y", 0}, {"width", 10}, {"height", 10}}}};
    for (size_t level = 0; level < 70; ++level)
        deep = {{"id", "deep:" + std::to_string(level)}, {"type", "GROUP"}, {"name", "Nested group"},
            {"absoluteBoundingBox", {{"x", 0}, {"y", 0}, {"width", 10}, {"height", 10}}}, {"children", json::array({deep})}};
    Reject([&] { FigmaImport::Convert(deep); });
}
struct Display {
    lv_display_t* value = nullptr;
    std::vector<uint32_t> pixels = std::vector<uint32_t>(640 * 480);
    Display() {
        value = lv_display_create(640, 480); CHECK(value);
        lv_display_set_color_format(value, LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_buffers(value, pixels.data(), nullptr, static_cast<uint32_t>(pixels.size() * 4), LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_flush_cb(value, [](lv_display_t* display, const lv_area_t*, uint8_t*) {lv_display_flush_ready(display);});
    }
    ~Display() { lv_display_delete(value); }
};
void DeleteRoots(const std::vector<lv_obj_t*>& objects) {
    std::vector<lv_obj_t*> roots;
    for (auto* object : objects) if (lv_obj_get_parent(object) == lv_screen_active()) roots.push_back(object);
    for (auto* object : roots) lv_obj_delete(object);
}
void LoadProject(const json& document) {
    auto selected = document;
    for (size_t screen = 0; screen < selected.at("screens").size(); ++screen) {
        Serialization::ProjectScreens::Select(selected, screen);
        auto loaded = Serialization::Project::FromJSON(selected);
        CHECK(loaded.objects.size() == selected.at("obj").size());
        bool heading = false;
        for (auto* object : loaded.objects)
            if (lv_obj_has_class(object, &lv_label_class) && std::string(lv_label_get_text(object)) == "Ready & connected — café") heading = true;
        if (screen == 0) CHECK(heading);
        lv_obj_update_layout(lv_screen_active());
        DeleteRoots(loaded.objects);
    }
}
void NativeGeometry() {
    Display display; auto source = Source();
    auto& card = source["document"]["children"][0]["children"][0]["children"][0];
    card["type"] = "FRAME";
    card["strokes"] = {{{"type", "SOLID"}, {"color", {{"r", 1}, {"g", 1}, {"b", 1}, {"a", 1}}}}};
    card["strokeWeight"] = 2; card["strokeAlign"] = "INSIDE";
    const auto imported = FigmaImport::Convert(source);
    auto loaded = Serialization::Project::FromJSON(imported.project);
    lv_obj_update_layout(lv_screen_active());
    bool found = false;
    for (auto* object : loaded.objects) {
        if (!lv_obj_has_class(object, &lv_label_class) || std::string(lv_label_get_text(object)) != "Ready & connected — café") continue;
        lv_area_t area{}; lv_obj_get_coords(object, &area);
        CHECK(area.x1 == 28 && area.y1 == 36); // Figma world position minus selected frame origin.
        found = true;
    }
    CHECK(found); DeleteRoots(loaded.objects);
}
Runtime LoadBytes(const void* bytes, size_t count) {
    lvgl_runtime_t* raw = nullptr; char error[1024]{};
    if (!lvgl_runtime_load_memory(bytes, count, nullptr, &raw, error, sizeof error)) throw std::runtime_error(error);
    CHECK(raw); return Runtime(raw, lvgl_runtime_destroy);
}
void Exports(const fs::path& output) {
    fs::create_directories(output); Display display;
    const auto result = FigmaImport::Convert(RasterSource());
    LoadProject(result.project);
    Write(output / "imported.json", result.project.dump(2));
    const auto bytes = Serialization::Protobuf::Encode(Serialization::EntityKind::Project, "project", result.project);
    {
        auto runtime = LoadBytes(bytes.data(), bytes.size());
        CHECK(lvgl_runtime_screen_count(runtime.get()) == 2);
        lv_refr_now(display.value);
    }
#if LVSERIAL_USE_XML
    const auto xml = Serialization::Xml::Encode(Serialization::EntityKind::Project, "project", result.project);
    CHECK(Serialization::Xml::Decode(xml).data == result.project);
    { auto runtime = LoadBytes(xml.data(), xml.size()); CHECK(lvgl_runtime_screen_count(runtime.get()) == 2); }
    XmlCodeGenerator xmlGenerator; CHECK(xmlGenerator.LoadProject(result.project));
    if (!xmlGenerator.ExportProject((output / "imported.xml").u8string())) throw std::runtime_error(xmlGenerator.GetLastError());
#endif
    CCodeGenerator c; CHECK(c.LoadProject(result.project));
    if (!c.ExportProject((output / "c").u8string())) throw std::runtime_error(c.GetLastError());
    CHECK(fs::file_size(output / "c/ui.c") > 0);
    MicroPythonCodeGenerator python; CHECK(python.LoadProject(result.project));
    if (!python.ExportProject((output / "micropython").u8string())) throw std::runtime_error(python.GetLastError());
    CHECK(fs::file_size(output / "micropython/ui.py") > 0);
    const auto exported = json::parse(Read(output / "micropython/ui.json"));
    CHECK(exported.at("screens").size() == 2);
    CHECK(Read(output / "c/ui.c").find("Ready & connected") != std::string::npos);
    Write(output / "bundle.figma.json", RasterSource().dump(2));
}
}
int main(int argc, char** argv) {
    try {
        CHECK(argc >= 2); lv_init(); const std::string test = argv[1];
        if (test == "mapping") Mapping(); else if (test == "formats") Formats();
        else if (test == "assets") Assets(); else if (test == "malformed") Malformed();
        else if (test == "geometry") NativeGeometry();
        else if (test == "exports" && argc == 3) Exports(fs::u8path(argv[2]));
        else throw std::runtime_error("Unknown Figma test case");
        lv_deinit(); std::cout << "Figma " << test << " checks passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
