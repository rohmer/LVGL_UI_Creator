#include <Serialization/CodeWidgets.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Project.h>
#include "../../CodeGeneration/CCodeGenerator.h"
#include "../../Runtime/include/lvgl_runtime.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Serialization;
namespace
{
void Check(bool condition, const std::string& error) { if (!condition) throw std::runtime_error(error); }
void Flush(lv_display_t* display, const lv_area_t*, uint8_t*) { lv_display_flush_ready(display); }
uint64_t Pixels(lv_obj_t* object)
{
    auto* buffer = lv_canvas_get_draw_buf(object);
    Check(buffer && buffer->data && buffer->data_size, "Encoded canvas has no pixels");
    uint64_t result = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < buffer->data_size; ++i) result = (result ^ buffer->data[i]) * UINT64_C(1099511628211);
    return result;
}
void Valid(lv_obj_t* object)
{
    const auto* type = CodeWidgets::Type(object);
    Check(type, "Code widget lost its native type");
    Check(std::string(type) == "barcode" ? lv_barcode_is_render_valid(object) : lv_qrcode_is_render_valid(object), "Native code encoder failed");
    (void)Pixels(object);
}
}
int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected code-widget fixture output directory");
        const auto directory = std::filesystem::u8path(argv[1]);
        std::filesystem::create_directories(directory);
        lv_init();auto* display = lv_display_create(640, 480);Check(display, "No display");
        std::vector<uint8_t> pixels(640 * 16 * 4);
        lv_display_set_buffers(display, pixels.data(), nullptr, static_cast<uint32_t>(pixels.size()), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, Flush);
        auto* previous = lv_screen_active();
        auto* barcode = ObjectSerializer::Create("barcode");auto* qr = ObjectSerializer::Create("qrcode");
        Check(barcode && qr, "Code widget factory failed");
        lv_obj_set_pos(barcode, 20, 40);lv_obj_set_pos(qr, 360, 40);
        CodeWidgets::Apply(barcode, {{"data", "87654321"}, {"darkColor", 0xff123456u}, {"lightColor", 0xfffafafau}});
        CodeWidgets::Apply(qr, {{"data", std::string("ESP\0Device", 10)}, {"size", 160}, {"quietZone", true}});
        Valid(barcode);Valid(qr);
        for (auto* object : {barcode, qr})
        {
            const auto original = ObjectSerializer::SerializeObject(object);
            const auto type = std::string(CodeWidgets::Type(object));
            const auto encodedPixels = Pixels(object);
            for (bool protobuf : {false, true})
            {
                auto* restored = protobuf ? ObjectSerializer::FromProtobuf(ObjectSerializer::ToProtobuf(object)) : ObjectSerializer::DeserializeObject(original);
                Check(restored, "Code widget restoration returned null");
                const auto restoredCode = ObjectSerializer::SerializeObject(restored).at(type);
                Check(restoredCode == original.at(type), type + (protobuf ? " Protobuf" : " JSON") + " payload/options did not round trip: " + json::diff(original.at(type), restoredCode).dump());
                Valid(restored);Check(Pixels(restored) == encodedPixels, "Round trip changed the encoded pixels");
                lv_obj_delete(restored);
            }
            auto modified = original.at(type);modified["darkColor"] = 0xff654321u;
            auto* originalBuffer = lv_canvas_get_draw_buf(object);
            CodeWidgets::Apply(object, modified);
            Check(lv_canvas_get_draw_buf(object) == originalBuffer && Pixels(object) != encodedPixels, "Palette update did not reuse its encoded canvas");
            CodeWidgets::Apply(object, original.at(type));
            Check(Pixels(object) == encodedPixels, "Palette restoration changed code geometry");
            std::vector<json> invalid;
            if (type == "barcode") invalid = {{{"data", ""}}, {{"data", std::string("bad\0data", 8)}}, {{"data", "caf\xc3\xa9"}}, {{"scale", 1.5}}, {{"scale", 0}}, {{"encoding", 2}}};
            else invalid = {{{"data", std::string(3000, 'a')}}, {{"size", 8}}, {{"size", 1.5}}, {{"data", std::string(200, 'x')}, {"size", 29}}};
            for (const auto& bad : invalid)
            {
                bool rejected = false;try { CodeWidgets::Apply(object, bad); } catch (const std::exception&) { rejected = true; }
                Check(rejected, "Invalid code properties were accepted");
                Check(ObjectSerializer::SerializeObject(object).at(type) == original.at(type), "Rejected edit changed saved code properties");
                Valid(object);Check(Pixels(object) == encodedPixels, "Rejected edit changed the previous code bitmap");
            }
            if (type == "barcode") Check(lv_barcode_update(object,"11223344") == LV_RESULT_OK,"Native barcode update failed");
            else Check(lv_qrcode_update(object,"native update",13) == LV_RESULT_OK,"Native QR update failed");
            Check(ObjectSerializer::SerializeObject(object).at(type).at("data") != original.at(type).at("data"),"Serializer ignored a native data update");
            CodeWidgets::Apply(object,original.at(type));
            Check(Pixels(object)==encodedPixels,"Restoring after a native update changed code content");
        }
        auto document = Project::ToJSON({barcode, qr}, previous, json::array(), 640, 480);
        document["runtimeTarget"] = "9.6";
        const auto binary = Protobuf::Encode(EntityKind::Project, "project", document);
        std::ofstream(directory / "fixture.json") << document.dump(2);
        std::ofstream stream(directory / "fixture.pb", std::ios::binary);stream.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));stream.close();
        CCodeGenerator generator;Check(generator.LoadProject(document), generator.GetLastError());
        json summary;Check(generator.Preflight(summary), generator.GetLastError());
        Check(generator.ExportProject(directory.u8string()), generator.GetLastError());
        for (const auto* target : {"9.5", "9.3", "9.2", "9.1", "8.3"})
        {
            generator.SetTargetVersion(target);
            Check(!generator.ExportProject((directory / (std::string("unsupported-") + target)).u8string()) && generator.GetLastError().find("9.6") != std::string::npos,
                  "Older target did not report code-widget capability limits");
        }
        for (int iteration = 0; iteration < 3; ++iteration)
        {
            lvgl_runtime_t* runtime = nullptr;char error[256];
            Check(lvgl_runtime_load_memory(binary.data(), binary.size(), nullptr, &runtime, error, sizeof(error)), error);
            for (int id = 0; id < 2; ++id)
            {
                auto* object = lvgl_runtime_object(runtime, 0, id);Check(object, "Runtime lost a code widget");Valid(object);
                const auto* type = id == 0 ? "barcode" : "qrcode";
                Check(ObjectSerializer::SerializeObject(object).at(type) == document.at("obj").at(id).at(type), "Runtime changed code content/options");
            }
            lv_refr_now(display);lvgl_runtime_destroy(runtime);Check(lv_screen_active() == previous, "Runtime did not restore the previous screen");
        }
        lv_obj_delete(barcode);lv_obj_delete(qr);lv_deinit();
        std::cout << "Code 128 and QR native rendering, validated edits, JSON/Protobuf, runtime and C generation passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
}
