#include "lvgl_runtime.h"
#include <Serialization/Protobuf.h>
#if LVSERIAL_USE_XML
#include <Serialization/Xml.h>
#endif
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#define CHECK(value) do { if (!(value)) throw std::runtime_error(#value); } while (false)
int main(int argc, char** argv)
{
    try {
        CHECK(argc == 2);
        using namespace Serialization;
        lv_init();
        auto* display = lv_display_create(240, 320);
        CHECK(display);
        auto* previous = lv_screen_active();
        const nlohmann::json project = {{"runtimeTarget", "9.6"},
            {"canvas", {{"width", 240}, {"height", 320}}},
            {"obj", {{{"oid", 1}, {"par", -1}, {"base", nlohmann::json::object()}, {"text", "XML & Protobuf"}}}}};
        char error[256]{};
        lvgl_runtime_t* runtime = nullptr;
        const auto protobuf = Protobuf::Encode(EntityKind::Project, "project", project);
        CHECK(lvgl_runtime_load_memory(protobuf.data(), protobuf.size(), nullptr, &runtime, error, sizeof(error)));
        CHECK(std::string(lv_label_get_text(lvgl_runtime_object(runtime, 0, 1))) == "XML & Protobuf");
        lvgl_runtime_destroy(runtime);
        CHECK(lv_screen_active() == previous);
        std::string xml;
#if LVSERIAL_USE_XML
        xml = Xml::Encode(EntityKind::Project, "project", project);
        std::ofstream(argv[1], std::ios::binary).write(xml.data(), xml.size());
#else
        std::ifstream input(argv[1], std::ios::binary);
        xml.assign(std::istreambuf_iterator<char>(input), {});
#endif
        CHECK(!xml.empty());
        runtime = reinterpret_cast<lvgl_runtime_t*>(1);
        const bool loaded = lvgl_runtime_load_memory(xml.data(), xml.size(), nullptr, &runtime, error, sizeof(error));
#if LVSERIAL_USE_XML
        CHECK(loaded);
        CHECK(std::string(lv_label_get_text(lvgl_runtime_object(runtime, 0, 1))) == "XML & Protobuf");
        lvgl_runtime_destroy(runtime);
        runtime = nullptr;
        CHECK(lvgl_runtime_load_memory_ex(xml.data(), xml.size(), nullptr,
            LVGL_RUNTIME_LAZY_SCREENS | LVGL_RUNTIME_BORROW_ASSETS, &runtime, error, sizeof(error)));
        xml.assign(xml.size(), 'x');
        CHECK(std::string(lv_label_get_text(lvgl_runtime_object(runtime, 0, 1))) == "XML & Protobuf");
        lvgl_runtime_destroy(runtime);
#else
        CHECK(!loaded && runtime == nullptr);
        CHECK(std::string(error).find("XML input is disabled") != std::string::npos);
#endif
        CHECK(lv_screen_active() == previous);
        lv_display_delete(display);
        lv_deinit();
        std::cout << "Minimal Protobuf profile and XML input gate passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
