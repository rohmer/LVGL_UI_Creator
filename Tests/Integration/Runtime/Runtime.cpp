#include "lvgl_runtime.h"
#include <Serialization/CustomWidgets.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Project.h>
#include <Serialization/Protobuf.h>
#include <Serialization/LVVariable.h>
#include <fstream>
#include <iostream>
#include <src/image/lv_image_decoder_private.h>
#include <stdexcept>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
            throw std::runtime_error(std::string("Line ") + std::to_string(__LINE__) + ": " #x);             \
    } while (0)
using namespace Serialization;
int main(int argc, char **argv)
{
    try
    {
        CHECK(argc == 3);
        std::ifstream file(argv[1], std::ios::binary);
        Binary bytes((std::istreambuf_iterator<char>(file)), {});
        auto document = Protobuf::Decode(bytes).data;
        lv_init();
        auto *display = lv_display_create(800, 600);
        auto *original = lv_screen_active();
        char error[512];
        lvgl_runtime_t *runtime = nullptr;
        CHECK(lvgl_runtime_load_memory(bytes.data(), bytes.size(), nullptr, &runtime, error, sizeof error));
        CHECK(runtime);
        CHECK(lvgl_runtime_screen_count(runtime) == document["screens"].size());
        for (size_t i = 0; i < document["screens"].size(); ++i)
            for (const auto &record : document["screens"][i]["obj"])
            {
                auto *object = lvgl_runtime_object(runtime, i, record["oid"]);
                CHECK(object);
                auto saved = ObjectSerializer::SerializeObject(object);
                for (const auto &field : record.items())
                    if (field.key() != "base" && field.key() != "oid" && field.key() != "par" &&
                        field.key() != "hostPath" && field.key() != "hostIndex")
                        CHECK(saved.contains(field.key()));
            }
        CHECK(lvgl_runtime_variable_count(runtime) == 7);
        lvgl_runtime_value_t value{};
        CHECK(lvgl_runtime_get_variable(runtime, "level", &value));
        CHECK(value.type == LVGL_RUNTIME_INTEGER && value.integer_value == 51);
        CHECK(lvgl_runtime_history_count(runtime, "level") == 2);
        CHECK(lvgl_runtime_history_get(runtime, "level", 0, &value) && value.integer_value == 50);
        value.type = LVGL_RUNTIME_INTEGER;
        value.integer_value = 72;
        CHECK(lvgl_runtime_set_variable(runtime, "level", &value));
        int bar = -1, textarea = -1, check = -1;
        size_t textarea_screen = 0;
        for (const auto &widget : document["screens"][0]["obj"])
        {
            if (widget.contains("bar"))
                bar = widget["oid"];
            if (widget.contains("switch"))
                check = widget["oid"];
        }
        for (const auto& record : document["variables"])
        {
            const auto definition = LVVariable::FromJSON(record);
            if (definition.name != "caption") continue;
            const auto bindings = LVVariable::Bindings(definition);
            CHECK(bindings.size() == 1);
            CHECK(bindings[0].property == "/textarea/text");
            textarea = bindings[0].objectId;
            textarea_screen = bindings[0].screen;
        }
        CHECK(textarea >= 0);
        CHECK(lv_bar_get_value(lvgl_runtime_object(runtime, 0, bar)) == 72);
        CHECK(lvgl_runtime_history_count(runtime, "level") == 2);
        value.type = LVGL_RUNTIME_STRING;
        value.string_value = "Runtime caption";
        CHECK(lvgl_runtime_set_variable(runtime, "caption", &value));
        auto *entry = lvgl_runtime_object(runtime, textarea_screen, textarea);
        CHECK(std::string(lv_textarea_get_text(entry)) == "Runtime caption");
        lv_textarea_set_text(entry, "Typed caption");
        lv_obj_send_event(entry, LV_EVENT_VALUE_CHANGED, nullptr);
        CHECK(lvgl_runtime_get_variable(runtime, "caption", &value) &&
              std::string(value.string_value) == "Typed caption");
        auto *toggle = lvgl_runtime_object(runtime, 0, check);
        lv_obj_remove_state(toggle, LV_STATE_CHECKED);
        lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, nullptr);
        CHECK(lvgl_runtime_get_variable(runtime, "enabled", &value) && !value.boolean_value);
        value.type = LVGL_RUNTIME_INTEGER;
        CHECK(!lvgl_runtime_set_variable(runtime, "enabled", &value));
        CHECK(lvgl_runtime_show_screen(runtime, 1));
        CHECK(lv_screen_active() == lvgl_runtime_screen(runtime, 1));
        CHECK(!lvgl_runtime_show_screen(runtime, 99));
        lvgl_runtime_destroy(runtime);
        CHECK(lv_screen_active() == original);
        auto *screen = lv_obj_create(nullptr);
        lv_screen_load(screen);
        std::vector<lv_obj_t *> objects;
        for (const auto &type : ObjectSerializer::SupportedTypes())
            if (type != "barcode" && type != "qrcode" && type != "advancedKeyboard") objects.push_back(ObjectSerializer::Create(type));
        auto project = Project::ToJSON(objects, screen, json::array());
        auto encoded = Protobuf::Encode(EntityKind::Project, "project", project);
        lv_screen_load(original);
        lv_obj_delete(screen);
        for (int i = 0; i < 3; ++i)
        {
            CHECK(lvgl_runtime_load_memory(encoded.data(), encoded.size(), nullptr, &runtime, error,
                                           sizeof error));
            for (size_t n = 0; n < objects.size(); ++n)
                CHECK(lvgl_runtime_object(runtime, 0, int(n)));
            lvgl_runtime_destroy(runtime);
            CHECK(lv_screen_active() == original);
        }
        {
            lv_fs_path_ex_t path;
            lv_fs_make_path_from_buffer(&path, LV_FS_MEMFS_LETTER, bytes.data(), (uint32_t)bytes.size(),
                                        "pb");
            CHECK(lvgl_runtime_load_file((const char *)&path, nullptr, &runtime, error, sizeof error));
            lvgl_runtime_destroy(runtime);
            CHECK(lv_screen_active() == original);
        }
        {
            std::ifstream file(argv[2], std::ios::binary);
            Binary assets((std::istreambuf_iterator<char>(file)), {});
            if (!lvgl_runtime_load_memory(assets.data(), assets.size(), nullptr, &runtime, error,
                                          sizeof error))
                throw std::runtime_error(error);
            auto *font = lvgl_runtime_font(runtime, "font_1");
            CHECK(font);
            CHECK(lv_obj_get_style_text_font(lvgl_runtime_object(runtime, 0, 0), LV_PART_MAIN) == font);
            CHECK(lv_obj_get_style_text_font(lvgl_runtime_object(runtime, 0, 1), LV_PART_MAIN) == font);
            auto *image = lvgl_runtime_image(runtime, "image_1");
            CHECK(image);
            CHECK(lv_image_get_src(lvgl_runtime_object(runtime, 0, 2)) == image);
            CHECK(lv_image_get_src(lvgl_runtime_object(runtime, 0, 6)) == image);
            CHECK(lv_imagebutton_get_src_middle(lvgl_runtime_object(runtime, 0, 7),
                                                LV_IMAGEBUTTON_STATE_PRESSED) == image);
            for (int id = 2; id <= 5; ++id)
            {
                lv_image_decoder_dsc_t decoded{};
                CHECK(lv_image_decoder_open(&decoded, lv_image_get_src(lvgl_runtime_object(runtime, 0, id)),
                                            nullptr) == LV_RESULT_OK);
                CHECK(decoded.decoded);
                lv_image_decoder_close(&decoded);
            }
            auto *animation = lvgl_runtime_object(runtime, 0, 4);
            auto *first = lv_image_get_src(animation);
            lv_tick_inc(150);
            lv_timer_handler();
            CHECK(lv_image_get_src(animation) != first);
            lvgl_runtime_destroy(runtime);
            CHECK(lv_screen_active() == original);
            for (int i = 0; i < 3; ++i)
            {
                CHECK(lvgl_runtime_load_memory(assets.data(), assets.size(), nullptr, &runtime, error,
                                               sizeof error));
                lvgl_runtime_destroy(runtime);
            }
        }
        auto bad = project;
        bad["obj"][0]["par"] = 0;
        encoded = Protobuf::Encode(EntityKind::Project, "project", bad);
        CHECK(!lvgl_runtime_load_memory(encoded.data(), encoded.size(), nullptr, &runtime, error,
                                        sizeof error));
        CHECK(!runtime && lv_screen_active() == original);
        bad = project;
        bad["variables"] =
            json::array({{{"var", {{"name", "broken"}, {"oid", 0}, {"property", "unsupported"}}}}});
        encoded = Protobuf::Encode(EntityKind::Project, "project", bad);
        CHECK(!lvgl_runtime_load_memory(encoded.data(), encoded.size(), nullptr, &runtime, error,
                                        sizeof error));
        CHECK(!runtime && lv_screen_active() == original);
        lvgl_runtime_options_t options{};
        options.max_input_bytes = 1;
        CHECK(!lvgl_runtime_load_memory(bytes.data(), bytes.size(), &options, &runtime, error, sizeof error));
        CHECK(!runtime);
        project["runtimeTarget"] = "9.5";
        encoded = Protobuf::Encode(EntityKind::Project, "project", project);
        CHECK(!lvgl_runtime_load_memory(encoded.data(), encoded.size(), nullptr, &runtime, error,
                                        sizeof error));
        CHECK(!runtime);
        CHECK(lv_screen_active() == original);
        CHECK(!lvgl_runtime_load_memory(bytes.data(), bytes.size() / 2, nullptr, &runtime, error,
                                        sizeof error));
        CHECK(lv_screen_active() == original);
        lv_display_delete(display);
        lv_deinit();
        std::cout << "LVGL 9.6 runtime project loading passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
