#include "catch.hpp"
#if __has_include(<RuntimeCompat.h>)
#include <RuntimeCompat.h>
#endif
#include "lvgl_runtime.h"
#include <Serialization/Base64.h>
#include <Serialization/LVFont.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Protobuf.h>
#include <cstring>
#include <limits>
#include <memory>
using namespace Serialization;
namespace
{
using Handle = std::unique_ptr<lvgl_runtime_t, decltype(&lvgl_runtime_destroy)>;
json Empty()
{
    return {
        {"runtimeTarget", lvgl_runtime_backend()}, {"obj", json::array()}, {"canvas", {{"width", 320}, {"height", 240}}}, {"variables", json::array()}};
}
Handle Load(const json &document, bool optimized = true, const lvgl_runtime_options_t *options = nullptr)
{
    auto bytes = Protobuf::Encode(EntityKind::Project, "project", document, optimized);
    char error[256];
    lvgl_runtime_t *runtime = nullptr;
    bool result =
        lvgl_runtime_load_memory(bytes.data(), bytes.size(), options, &runtime, error, sizeof error);
    INFO(error);
    REQUIRE(result);
    return Handle(runtime, lvgl_runtime_destroy);
}
void Reject(const json &document)
{
    INFO(document.dump());
    auto *previous = lv_screen_active();
    auto bytes = Protobuf::Encode(EntityKind::Project, "project", document);
    char error[256];
    lvgl_runtime_t *runtime = (lvgl_runtime_t *)1;
    CHECK_FALSE(lvgl_runtime_load_memory(bytes.data(), bytes.size(), nullptr, &runtime, error, sizeof error));
    CHECK(runtime == nullptr);
    CHECK(error[0] != 0);
    CHECK(lv_screen_active() == previous);
}
json Var(std::string name, int type, json value, int history = 2)
{
    return {{"var", {{"name", name}, {"dataType", type}, {"default", value}, {"history", history}}}};
}
} // namespace
TEST_CASE("Runtime validates project envelopes and hierarchies", "[runtime][validation]")
{
    auto original = Empty();
    std::vector<json> invalid = {nullptr, 1, "text", json::array(), json::object()};
    for (auto document : invalid)
    {
        CAPTURE(document);
        Reject(document);
    }
    for (int version : {-1, 0, 3, 100})
    {
        auto document = original;
        document["formatVersion"] = version;
        Reject(document);
    }
    for (const char *target : {"9.6", "9.5", "9.3", "9.2", "9.1", "8.3", "invalid"})
    {
        if(std::string(target)==lvgl_runtime_backend())continue;
        auto document = original;
        document["runtimeTarget"] = target;
        Reject(document);
    }
    for (const char *version : {"8.3.11", "9.5.0", "9.60.0", "10.0.0"})
    {
        auto document = original;
        document["lvglVersion"] = version;
        Reject(document);
    }
    for (auto screens : {json::array(), json::object(), json(42)})
    {
        auto document = original;
        document["screens"] = screens;
        Reject(document);
    }
    {
        auto document = original;
        document["obj"] = json::object();
        Reject(document);
    }
    {
        auto document = original;
        document["obj"] = {{{"base", nullptr}, {"oid", 0}}};
        Reject(document);
    }
    auto object = json{{"base", json::object()}, {"oid", 0}, {"par", -1}};
    for (int parent : {0, 1, 999})
    {
        auto document = original;
        auto record = object;
        record["par"] = parent;
        document["obj"] = {record};
        Reject(document);
    }
    {
        auto document = original;
        document["obj"] = {object, object};
        Reject(document);
    }
    {
        auto document = original;
        document["obj"] = json::array({object});
        REQUIRE(document["obj"].is_array());
        document["obj"][0]["oid"] = -1;
        Reject(document);
    }
    {
        auto document = original;
        document["activeScreen"] = 1;
        Reject(document);
    }
    for (int dimension : {0, -1, 32768})
    {
        auto document = original;
        document["canvas"]["width"] = dimension;
        Reject(document);
    }
    for (auto path : {json::array({-1}), json::array({"bad"}), json::object(), json::array({2147483648LL})})
    {
        auto document = original;
        auto child = object;
        child["oid"] = 1;
        child["par"] = 0;
        child["hostPath"] = path;
        document["obj"] = json::array({object, child});
        Reject(document);
    }
    {
        auto document = original;
        auto child = object;
        child["oid"] = 1;
        child["par"] = 0;
        child["hostPath"] = {999};
        document["obj"] = {object, child};
        Reject(document);
    }
}
TEST_CASE("Runtime rejects every truncated prefix and wrong entity kind", "[runtime][wire]")
{
    auto bytes = Protobuf::Encode(EntityKind::Project, "project", Empty());
    auto *previous = lv_screen_active();
    for (size_t size = 0; size < bytes.size(); ++size)
    {
        lvgl_runtime_t *runtime = nullptr;
        char error[32];
        CHECK_FALSE(lvgl_runtime_load_memory(bytes.data(), size, nullptr, &runtime, error, sizeof error));
        CHECK(runtime == nullptr);
        CHECK(lv_screen_active() == previous);
    }
    for (auto kind : {EntityKind::Widget, EntityKind::Screen, EntityKind::Variable, EntityKind::Style,
                      EntityKind::Font, EntityKind::Asset})
    {
        auto wrong = Protobuf::Encode(kind, "project", Empty());
        lvgl_runtime_t *runtime = nullptr;
        char error[128];
        CHECK_FALSE(
            lvgl_runtime_load_memory(wrong.data(), wrong.size(), nullptr, &runtime, error, sizeof error));
        CHECK(runtime == nullptr);
    }
    for (bool optimized : {false, true})
    {
        auto runtime = Load(Empty(), optimized);
        CHECK(lvgl_runtime_screen_count(runtime.get()) == 1);
    }
}
TEST_CASE("Runtime loading options and null API inputs are safe", "[runtime][api]")
{
    auto bytes = Protobuf::Encode(EntityKind::Project, "project", Empty());
    char error[64];
    lvgl_runtime_t *raw = nullptr;
    lvgl_runtime_options_t options{};
    options.max_input_bytes = bytes.size() - 1;
    CHECK_FALSE(lvgl_runtime_load_memory(bytes.data(), bytes.size(), &options, &raw, error, sizeof error));
    CHECK(raw == nullptr);
    options.max_input_bytes = bytes.size();
    auto *original = lv_screen_active();
    {
        auto runtime = Load(Empty(), true, &options);
        CHECK(lv_screen_active() == original);
    }
    CHECK(lv_screen_active() == original);
    CHECK_FALSE(lvgl_runtime_load_memory(bytes.data(), bytes.size(), nullptr, nullptr, error, sizeof error));
    CHECK_FALSE(lvgl_runtime_load_file("Z:/not-present.pb", nullptr, &raw, error, sizeof error));
    CHECK(raw == nullptr);
    CHECK(lvgl_runtime_screen_count(nullptr) == 0);
    CHECK(lvgl_runtime_screen(nullptr, 0) == nullptr);
    CHECK_FALSE(lvgl_runtime_show_screen(nullptr, 0));
    CHECK(lvgl_runtime_object(nullptr, 0, 0) == nullptr);
    CHECK(lvgl_runtime_font(nullptr, "x") == nullptr);
    CHECK(lvgl_runtime_image(nullptr, "x") == nullptr);
    CHECK(lvgl_runtime_variable_count(nullptr) == 0);
    CHECK(lvgl_runtime_variable_name(nullptr, 0) == nullptr);
    CHECK_FALSE(lvgl_runtime_get_variable(nullptr, "x", nullptr));
    CHECK_FALSE(lvgl_runtime_set_variable(nullptr, "x", nullptr));
    CHECK(lvgl_runtime_history_count(nullptr, "x") == 0);
    CHECK_FALSE(lvgl_runtime_history_get(nullptr, "x", 0, nullptr));
    lvgl_runtime_destroy(nullptr);
    char one = 'X';
    CHECK_FALSE(lvgl_runtime_load_memory(nullptr, 0, nullptr, &raw, &one, 1));
    CHECK(one == 0);
    CHECK_FALSE(lvgl_runtime_load_memory(nullptr, 0, nullptr, &raw, nullptr, 0));
}
TEST_CASE("Runtime variables enforce types, history bounds and ownership", "[runtime][variables]")
{
    auto document = Empty();
    document["variables"] = {Var("text", 0, "Initial"), Var("integer", 1, INT64_MIN),
                             Var("boolean", 2, true, 1), Var("float", 3, 0.5, 0), Var("date", 4, INT64_MAX)};
    auto runtime = Load(document);
    CHECK(lvgl_runtime_variable_count(runtime.get()) == 5);
    lvgl_runtime_value_t value{};
    const char *names[] = {"text", "integer", "boolean", "float", "date"};
    for (int type = 0; type < 5; ++type)
    {
        REQUIRE(lvgl_runtime_get_variable(runtime.get(), names[type], &value));
        CHECK(value.type == type);
        CHECK(std::string(lvgl_runtime_variable_name(runtime.get(), type)) == names[type]);
        CHECK(lvgl_runtime_history_count(runtime.get(), names[type]) == 0);
        for (int wrong = 0; wrong < 5; ++wrong)
            if (type != wrong)
            {
                value.type = (lvgl_runtime_value_type_t)wrong;
                CHECK_FALSE(lvgl_runtime_set_variable(runtime.get(), names[type], &value));
            }
    }
    std::string transient = "Owned UTF-8 \xc3\xa9 text";
    value = {};
    value.type = LVGL_RUNTIME_STRING;
    value.string_value = transient.c_str();
    CHECK(lvgl_runtime_set_variable(runtime.get(), "text", &value));
    transient = "Overwritten";
    CHECK(lvgl_runtime_get_variable(runtime.get(), "text", &value));
    CHECK(std::string(value.string_value) == "Owned UTF-8 \xc3\xa9 text");
    CHECK(lvgl_runtime_set_variable(runtime.get(), "text", &value));
    value = {};
    value.type = LVGL_RUNTIME_INTEGER;
    for (int n = 0; n < 10; ++n)
    {
        value.integer_value = n;
        CHECK(lvgl_runtime_set_variable(runtime.get(), "integer", &value));
    }
    CHECK(lvgl_runtime_history_count(runtime.get(), "integer") == 2);
    CHECK(lvgl_runtime_history_get(runtime.get(), "integer", 0, &value));
    CHECK(value.integer_value == 8);
    CHECK_FALSE(lvgl_runtime_history_get(runtime.get(), "integer", 2, &value));
    value = {};
    value.type = LVGL_RUNTIME_FLOAT;
    for (int n = 0; n < 20; ++n)
    {
        value.real_value = n / 2.0;
        CHECK(lvgl_runtime_set_variable(runtime.get(), "float", &value));
    }
    CHECK(lvgl_runtime_history_count(runtime.get(), "float") == 20);
    for (double bad : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()})
    {
        value.real_value = bad;
        CHECK_FALSE(lvgl_runtime_set_variable(runtime.get(), "float", &value));
    }
    CHECK(lvgl_runtime_history_count(runtime.get(), "float") == 20);
    CHECK_FALSE(lvgl_runtime_get_variable(runtime.get(), "missing", &value));
    CHECK_FALSE(lvgl_runtime_get_variable(runtime.get(), "text", nullptr));
    CHECK_FALSE(lvgl_runtime_set_variable(runtime.get(), "text", nullptr));
    value = {};
    value.type = LVGL_RUNTIME_STRING;
    CHECK_FALSE(lvgl_runtime_set_variable(runtime.get(), "text", &value));
}
#if LV_USE_SLIDER && LV_USE_LABEL && LV_USE_TEXTAREA
TEST_CASE("Runtime variables fan out across directional bindings without feedback histories", "[runtime][variables]")
{
    auto document=Empty();
    auto slider=[](int id,int value){return json{{"oid",id},{"par",-1},{"base",json::object()},{"slider",{{"min",0},{"max",300},{"value",value}}}};};
    document["obj"]=json::array({slider(0,0),slider(1,0),slider(2,77),
        {{"oid",3},{"par",-1},{"base",json::object()},{"text","Caption"}},
        {{"oid",4},{"par",-1},{"base",json::object()},{"textarea",{{"text","Before"}}}},
        {{"oid",5},{"par",-1},{"base",json::object()},{"textarea",{{"text","Before"}}}}});
    auto counter=Var("counter",1,10,8);counter["var"]["code"]={{"cType","uint8_t"},{"symbol","device_counter"}};
    counter["var"]["bindings"]=json::array({{{"oid",3},{"property","text"},{"direction","to-widget"}},
        {{"oid",0},{"property","value"}},{{"oid",0},{"property","checked"}},{{"oid",1},{"property","value"},{"direction","to-widget"}},
        {{"oid",2},{"property","value"},{"direction","from-widget"}}});
    auto text=Var("text",0,"Initial",4);text["var"]["bindings"]=json::array({{{"oid",4},{"property","text"}},{{"oid",5},{"property","text"}}});
    document["variables"]=json::array({counter,text});auto runtime=Load(document);
    auto object=[&](int id){return lvgl_runtime_object(runtime.get(),0,id);};
    CHECK(lv_slider_get_value(object(0))==10);CHECK(lv_slider_get_value(object(1))==10);CHECK(lv_slider_get_value(object(2))==77);
    lv_slider_set_value(object(0),23,LV_ANIM_OFF);lv_obj_send_event(object(0),LV_EVENT_VALUE_CHANGED,nullptr);
    lvgl_runtime_value_t value{};REQUIRE(lvgl_runtime_get_variable(runtime.get(),"counter",&value));CHECK(value.integer_value==23);
    CHECK(lv_slider_get_value(object(1))==23);CHECK(std::string(lv_label_get_text(object(3)))=="23");
    CHECK(lvgl_runtime_history_count(runtime.get(),"counter")==1);
    lv_slider_set_value(object(1),88,LV_ANIM_OFF);lv_obj_send_event(object(1),LV_EVENT_VALUE_CHANGED,nullptr);
    REQUIRE(lvgl_runtime_get_variable(runtime.get(),"counter",&value));CHECK(value.integer_value==23);CHECK(lvgl_runtime_history_count(runtime.get(),"counter")==1);
    lv_slider_set_value(object(2),45,LV_ANIM_OFF);lv_obj_send_event(object(2),LV_EVENT_VALUE_CHANGED,nullptr);
    REQUIRE(lvgl_runtime_get_variable(runtime.get(),"counter",&value));CHECK(value.integer_value==45);CHECK(lv_slider_get_value(object(0))==45);CHECK(lv_slider_get_value(object(1))==45);
    CHECK(lvgl_runtime_history_count(runtime.get(),"counter")==2);
    value.integer_value=256;CHECK_FALSE(lvgl_runtime_set_variable(runtime.get(),"counter",&value));
    CHECK(std::string(lv_label_get_text(object(3)))=="45");CHECK(lvgl_runtime_history_count(runtime.get(),"counter")==2);
    lv_slider_set_value(object(0),256,LV_ANIM_OFF);lv_obj_send_event(object(0),LV_EVENT_VALUE_CHANGED,nullptr);
    CHECK(lv_slider_get_value(object(0))==45);CHECK(lvgl_runtime_history_count(runtime.get(),"counter")==2);
    lv_textarea_set_text(object(4),"UTF-8 peers \xc3\xa9");lv_obj_send_event(object(4),LV_EVENT_VALUE_CHANGED,nullptr);
    CHECK(std::string(lv_textarea_get_text(object(5)))=="UTF-8 peers \xc3\xa9");
    const auto textHistory=lvgl_runtime_history_count(runtime.get(),"text");CHECK(textHistory<=2);CHECK(textHistory>0);
    lv_obj_delete(object(0));value.type=LVGL_RUNTIME_INTEGER;value.integer_value=19;
    REQUIRE(lvgl_runtime_set_variable(runtime.get(),"counter",&value));CHECK(lv_slider_get_value(object(1))==19);
    CHECK(lv_slider_get_value(object(2))==45);CHECK(std::string(lv_label_get_text(object(3)))=="19");
}
#endif
#if LV_USE_TEXTAREA && LV_USE_LABEL
TEST_CASE("Runtime propagation suppresses authored actions and rejects reentrant public setters", "[runtime][variables][interactions]")
{
    auto document=Empty();auto entry=json{{"oid",0},{"par",-1},{"base",json::object()},{"textarea",{{"text","Before"}}}};
    entry["base"]["events"]=json::array({{{"event","value_changed"},{"action","set_variable"},{"variable","text"},{"value","Action replaced"}},
        {{"event","value_changed"},{"action","increment_variable"},{"variable","events"},{"value",1}}});
    document["obj"]=json::array({entry,{{"oid",1},{"par",-1},{"base",json::object()},{"text","Before"}}});
    auto text=Var("text",0,"Initial",8);text["var"]["bindings"]=json::array({{{"oid",0},{"property","text"}},{{"oid",1},{"property","text"}}});
    document["variables"]=json::array({text,Var("events",1,0,8)});auto runtime=Load(document);
    struct Probe {lvgl_runtime_t* runtime;bool attempted=false,accepted=false;} probe{runtime.get()};
    auto callback=+[](lv_event_t* event){
        auto* state=static_cast<Probe*>(lv_event_get_user_data(event));if(state->attempted)return;state->attempted=true;
        lvgl_runtime_value_t value{};value.type=LVGL_RUNTIME_STRING;value.string_value="Nested replacement";
        state->accepted=lvgl_runtime_set_variable(state->runtime,"text",&value);
    };
    auto* input=lvgl_runtime_object(runtime.get(),0,0);lv_obj_add_event_cb(input,callback,LV_EVENT_VALUE_CHANGED,&probe);
    lvgl_runtime_value_t value{};value.type=LVGL_RUNTIME_STRING;value.string_value="Outer value";
    REQUIRE(lvgl_runtime_set_variable(runtime.get(),"text",&value));CHECK(probe.attempted);CHECK_FALSE(probe.accepted);
    CHECK(std::string(lv_textarea_get_text(input))=="Outer value");CHECK(std::string(lv_label_get_text(lvgl_runtime_object(runtime.get(),0,1)))=="Outer value");
    CHECK(lvgl_runtime_history_count(runtime.get(),"text")==1);REQUIRE(lvgl_runtime_get_variable(runtime.get(),"events",&value));CHECK(value.integer_value==0);
    lv_obj_remove_event_cb_with_user_data(input,callback,&probe);
    lv_textarea_set_text(input,"User edit");
    CHECK(std::string(lv_textarea_get_text(input))=="Action replaced");
    CHECK(std::string(lv_label_get_text(lvgl_runtime_object(runtime.get(),0,1)))=="Action replaced");
    REQUIRE(lvgl_runtime_get_variable(runtime.get(),"events",&value));CHECK(value.integer_value==1);
}
#endif
TEST_CASE("Runtime enforces configured C numeric storage before committing updates", "[runtime][variables]")
{
    auto document=Empty();auto narrow=Var("narrow",1,7);narrow["var"]["code"]={{"cType","int8_t"}};
    auto precise=Var("precise",3,0.1);precise["var"]["code"]={{"cType","float"}};
    document["variables"]=json::array({narrow,precise});auto runtime=Load(document);lvgl_runtime_value_t value{};
    REQUIRE(lvgl_runtime_get_variable(runtime.get(),"precise",&value));CHECK(value.real_value==double(float(0.1)));
    value.type=LVGL_RUNTIME_INTEGER;value.integer_value=128;CHECK_FALSE(lvgl_runtime_set_variable(runtime.get(),"narrow",&value));
    REQUIRE(lvgl_runtime_get_variable(runtime.get(),"narrow",&value));CHECK(value.integer_value==7);CHECK(lvgl_runtime_history_count(runtime.get(),"narrow")==0);
    value.type=LVGL_RUNTIME_FLOAT;value.real_value=1e100;CHECK_FALSE(lvgl_runtime_set_variable(runtime.get(),"precise",&value));
    CHECK(lvgl_runtime_history_count(runtime.get(),"precise")==0);
}
TEST_CASE("Runtime rejects invalid variable definitions and bindings transactionally",
          "[runtime][variables][rollback]")
{
    for (int type : {-1, 5, 100})
    {
        auto document = Empty();
        document["variables"] = {Var("invalid", type, 0)};
        Reject(document);
    }
    for (int history : {-1, 65536})
    {
        auto document = Empty();
        document["variables"] = {Var("invalid", 1, 0, history)};
        Reject(document);
    }
    for (auto value : {json("wrong"), json(true), json::object(), json::array()})
    {
        auto document = Empty();
        document["variables"] = {Var("bad", 1, value)};
        Reject(document);
    }
    for (const auto &name : {std::string(), std::string("has\0NUL", 7)})
    {
        auto document = Empty();
        document["variables"] = {Var(name, 1, 0)};
        Reject(document);
    }
    {
        auto document = Empty();
        document["variables"] = {Var("same", 1, 0), Var("same", 2, true)};
        Reject(document);
    }
    {
        auto document = Empty();
        document["variables"] = {Var("bad", 0, std::string("has\0NUL", 7))};
        Reject(document);
    }
    for (const char *property : {"/bar/value", "unsupported"})
    {
        auto document = Empty();
        auto variable = Var("bad", 1, 0);
        variable["var"]["oid"] = 99;
        variable["var"]["property"] = property;
        document["variables"] = {variable};
        Reject(document);
    }
    {
        auto document = Empty();
        document["obj"] = {{{"base", json::object()}, {"bar", {{"value", 5}}}, {"oid", 0}}};
        auto variable = Var("bad", 1, 0);
        variable["var"]["oid"] = 0;
        variable["var"]["property"] = "unsupported";
        document["variables"] = {variable};
        Reject(document);
    }
    {
        auto document=Empty();document["obj"]=json::array({{{"oid",0},{"par",-1},{"base",json::object()},{"text","Output only"}}});
        auto variable=Var("bad-direction",1,0);
        variable["var"]["bindings"]=json::array({{{"oid",0},{"property","text"},{"direction","from-widget"}}});
        document["variables"]=json::array({variable});Reject(document);
    }
    {
        auto document=Empty();document["obj"]=json::array({{{"oid",0},{"par",-1},{"base",json::object()},{"slider",json::object()}}});
        auto variable=Var("bad-checked-input",1,0);variable["var"]["bindings"]=json::array({{{"oid",0},{"property","checked"},{"direction","from-widget"}}});
        document["variables"]=json::array({variable});Reject(document);
    }
}
TEST_CASE("Runtime safe handles survive manual deletion and independent projects", "[runtime][lifetime]")
{
    auto *previous = lv_screen_active();
    auto document = Empty();
    document["obj"] = {{{"base", json::object()}, {"text", "Kept"}, {"oid", 7}, {"par", -1}}};
    for (int cycle = 0; cycle < 50; ++cycle)
    {
        auto first = Load(document);
        auto second = Load(document);
        auto *screen = lvgl_runtime_screen(second.get(), 0);
        CHECK(lv_screen_active() == screen);
        first.reset();
        CHECK(lv_screen_active() == screen);
        lv_obj_delete(lvgl_runtime_object(second.get(), 0, 7));
        CHECK(lvgl_runtime_object(second.get(), 0, 7) == nullptr);
        second.reset();
        CHECK(lv_screen_active() != nullptr);
        auto *blank = lv_screen_active();
        lv_screen_load(previous);
        if (blank != previous)
            lv_obj_delete(blank);
    }
    for (int i = 0; i < 10; ++i)
    {
        auto runtime = Load(document);
        CHECK(lvgl_runtime_object(runtime.get(), 0, 7));
        runtime.reset();
        CHECK(lv_screen_active() == previous);
    }
    {
        auto runtime = Load(document);
        auto *own = lvgl_runtime_screen(runtime.get(), 0);
        lv_screen_load(previous);
        lv_obj_delete(own);
        CHECK(lvgl_runtime_screen(runtime.get(), 0) == nullptr);
        CHECK_FALSE(lvgl_runtime_show_screen(runtime.get(), 0));
    }
}
TEST_CASE("Runtime rejects bad resource references and incomplete image payloads", "[runtime][assets]")
{
    for (const char *reference : {"fontId", "imageId"})
    {
        auto document = Empty();
        document["obj"] = {{{"base", {{reference, "missing"}}}, {"oid", 0}}};
        Reject(document);
    }
    for (const char *reference : {"projectFont", "projectImage"})
    {
        auto document = Empty();
        document["extra"] = {{reference, "missing"}};
        Reject(document);
    }
    for (const auto &bytes : {std::string(), std::string("abc")})
    {
        auto document = Empty();
        json frame = {{"w", 1},
                      {"h", 1},
                      {"stride", 4},
                      {"cf", LV_COLOR_FORMAT_ARGB8888},
                      {"data", base64_encode((const unsigned char *)bytes.data(), bytes.size())}};
        json asset = {{"sourceData", "unused"}, {"deviceFrames", json::array({frame})}};
        document["images"] = json::array({json{{"id", "image_1"}, {"name", "Image"}, {"image", asset}}});
        Reject(document);
    }
    for (auto metadata : {json{{"w", 0}}, json{{"h", 65536}}, json{{"stride", 0}}, json{{"cf", 0}},
                          json{{"formatVersion", 8}}, json{{"flags", LV_IMAGE_FLAGS_COMPRESSED}}})
    {
        auto document = Empty();
        json frame = {
            {"w", 1}, {"h", 1}, {"stride", 4}, {"cf", LV_COLOR_FORMAT_ARGB8888}, {"data", "AAAAAA=="}};
        frame.update(metadata);
        document["images"] = {
            {{"id", "image_1"},
             {"name", "Image"},
             {"image", {{"sourceData", "unused"}, {"deviceFrames", json::array({frame})}}}}};
        Reject(document);
    }
}

TEST_CASE("Catalog resources stay shared and compact inside runtime widgets", "[runtime][assets][fonts]")
{
    auto document = Empty();
    auto font = LVFont::ToJSON(*LV_FONT_DEFAULT);
    document["fonts"] = json::array({json{{"id", "font_1"}, {"name", "Body"}, {"font", font}}});
    json frame = {{"w", 1}, {"h", 1}, {"stride", 4}, {"cf", LV_COLOR_FORMAT_ARGB8888}, {"data", "AAAA/w=="}};
    json image = {{"sourceData", "unused"}, {"deviceFrames", json::array({frame})}};
    document["images"] = json::array({json{{"id", "image_1"}, {"name", "Icon"}, {"image", image}}});
    json base = {
        {"fontId", "font_1"},
        {"nativeStyles",
         json::array({json{{"selector", 0}, {"values", {{"text_font", {{"projectFont", "font_1"}}}}}}})}};
    document["obj"] = json::array(
        {json{{"base", base}, {"text", "First"}, {"oid", 0}},
         json{{"base", base}, {"text", "Second"}, {"oid", 1}},
         json{{"base", json::object()}, {"img", {{"asset", {{"projectImage", "image_1"}}}}}, {"oid", 2}},
         json{{"base", json::object()}, {"img", {{"asset", {{"projectImage", "image_1"}}}}}, {"oid", 3}}});
    auto first = Load(document);
    auto *loadedFont = lvgl_runtime_font(first.get(), "font_1");
    CHECK(loadedFont);
    CHECK(lv_obj_get_style_text_font(lvgl_runtime_object(first.get(), 0, 0), LV_PART_MAIN) == loadedFont);
    CHECK(lv_obj_get_style_text_font(lvgl_runtime_object(first.get(), 0, 1), LV_PART_MAIN) == loadedFont);
    auto *metadata =
        static_cast<ObjectUserData *>(lv_obj_get_user_data(lvgl_runtime_object(first.get(), 0, 0)));
    CHECK(metadata->objectJson["base"]["nativeStyles"][0]["values"]["text_font"].contains("projectFont"));
    auto *loadedImage = lvgl_runtime_image(first.get(), "image_1");
    CHECK(lv_image_get_src(lvgl_runtime_object(first.get(), 0, 2)) == loadedImage);
    CHECK(lv_image_get_src(lvgl_runtime_object(first.get(), 0, 3)) == loadedImage);
    metadata = static_cast<ObjectUserData *>(lv_obj_get_user_data(lvgl_runtime_object(first.get(), 0, 2)));
    CHECK(metadata->objectJson["img"]["asset"].contains("projectImage"));
    auto second = Load(document);
    CHECK(lvgl_runtime_font(second.get(), "font_1") != loadedFont);
    first.reset();
    lv_font_glyph_dsc_t glyph{};
    CHECK(lv_font_get_glyph_dsc(lvgl_runtime_font(second.get(), "font_1"), &glyph, 'A', 0));
}
TEST_CASE("Malformed font metrics and cmap tables are rejected", "[runtime][fonts][validation]")
{
    auto original = LVFont::ToJSON(*LV_FONT_DEFAULT);
    for (auto metadata : {json{{"lineHeight", 0}}, json{{"lineHeight", 32768}}, json{{"lineHeight", 2.5}}})
    {
        auto font = original;
        font.update(metadata);
        auto document = Empty();
        document["fonts"] = json::array({json{{"id", "font_1"}, {"name", "Body"}, {"font", font}}});
        Reject(document);
    }
    for (auto metadata : {json{{"bpp", 0}}, json{{"bpp", 3}}, json{{"bitmapFmt", 99}}, json{{"stride", 256}},
                          json{{"GlyphDsc", json::array()}}})
    {
        auto font = original;
        font["dsc"].update(metadata);
        auto document = Empty();
        document["fonts"] = json::array({json{{"id", "font_1"}, {"name", "Body"}, {"font", font}}});
        Reject(document);
    }
    {
        auto font = original;
        font["dsc"]["GlyphDsc"][1]["bitmapIdx"] = 999999;
        auto document = Empty();
        document["fonts"] = json::array({json{{"id", "font_1"}, {"name", "Body"}, {"font", font}}});
        Reject(document);
    }
    {
        auto font = original;
        font["dsc"]["cmaps"][0]["glyphIDstart"] = 999999;
        auto document = Empty();
        document["fonts"] = json::array({json{{"id", "font_1"}, {"name", "Body"}, {"font", font}}});
        Reject(document);
    }
}

TEST_CASE("Backends reject properties without a native equivalent", "[runtime][capabilities]") {
    std::string backend=lvgl_runtime_backend();
    auto object=json{{"base",json::object()},{"oid",0}};
    if(backend!="9.6") {auto d=Empty();object["base"]["flags"]={{"radio_button",true}};d["obj"]=json::array({object});Reject(d);object["base"]["flags"]["radio_button"]=false;d["obj"]=json::array({object});auto runtime=Load(d);CHECK(lvgl_runtime_object(runtime.get(),0,0));}
    if(backend=="9.1" || backend=="9.2" || backend=="8.3") {auto d=Empty();object=json{{"base",json::object()},{"slider",{{"orientation",1}}},{"oid",0}};d["obj"]=json::array({object});Reject(d);}
    if(backend=="8.3") {auto d=Empty();object=json{{"base",json::object()},{"arc",{{"knobOffset",1}}},{"oid",0}};d["obj"]=json::array({object});Reject(d);}
}

TEST_CASE("Runtime bounded histories wrap chronologically and retain initial values only once", "[runtime][variables][history]")
{
    auto document = Empty();
    auto bounded = Var("bounded", 1, -1, 3);
    bounded["var"]["value"] = {1,2,3,4,5,6,7,8};
    auto single = Var("single", 0, "default", 1);
    single["var"]["value"] = {"first", "last"};
    document["variables"] = {bounded, single, Var("unlimited", 1, 0, 0), Var("empty", 1, 5, 1)};
    auto runtime = Load(document);
    lvgl_runtime_value_t value{};
    REQUIRE(lvgl_runtime_history_count(runtime.get(), "bounded") == 3);
    for (size_t i = 0; i < 3; ++i) {
        REQUIRE(lvgl_runtime_history_get(runtime.get(), "bounded", i, &value));
        CHECK(value.integer_value == int64_t(i + 6));
    }
    CHECK(lvgl_runtime_history_count(runtime.get(), "empty") == 0);
    CHECK_FALSE(lvgl_runtime_history_get(runtime.get(), "empty", 0, &value));
    REQUIRE(lvgl_runtime_history_get(runtime.get(), "single", 0, &value));
    CHECK(std::string(value.string_value) == "last");
    CHECK_FALSE(lvgl_runtime_history_get(runtime.get(), "single", 1, &value));
    for (int64_t number = 9; number < 39; ++number) {
        value = {}; value.type = LVGL_RUNTIME_INTEGER; value.integer_value = number;
        REQUIRE(lvgl_runtime_set_variable(runtime.get(), "bounded", &value));
        REQUIRE(lvgl_runtime_set_variable(runtime.get(), "unlimited", &value));
        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(lvgl_runtime_history_get(runtime.get(), "bounded", i, &value));
            CHECK(value.integer_value == number - 2 + int64_t(i));
        }
    }
    CHECK(lvgl_runtime_history_count(runtime.get(), "unlimited") == 30);
    for (size_t i = 0; i < 30; ++i) {
        REQUIRE(lvgl_runtime_history_get(runtime.get(), "unlimited", i, &value));
        CHECK(value.integer_value == int64_t(i + 9));
    }
    for (int iteration = 0; iteration < 5; ++iteration) {
        std::string text(300, char('a' + iteration)); text += " \xc3\xa9";
        value = {}; value.type = LVGL_RUNTIME_STRING; value.string_value = text.c_str();
        REQUIRE(lvgl_runtime_set_variable(runtime.get(), "single", &value));
        CHECK(lvgl_runtime_history_count(runtime.get(), "single") == 1);
        REQUIRE(lvgl_runtime_history_get(runtime.get(), "single", 0, &value));
        CHECK(std::string(value.string_value) == text);
        REQUIRE(lvgl_runtime_get_variable(runtime.get(), "single", &value));
        CHECK(std::string(value.string_value) == text);
    }
    value = {}; value.type = LVGL_RUNTIME_INTEGER; value.integer_value = 123;
    CHECK_FALSE(lvgl_runtime_set_variable(runtime.get(), "single", &value));
    REQUIRE(lvgl_runtime_history_get(runtime.get(), "single", 0, &value));
    CHECK(std::string(value.string_value) == std::string(300, 'e') + " \xc3\xa9");
}

TEST_CASE("Runtime hierarchy validation handles shared deep paths and long cycles", "[runtime][validation]")
{
    auto document = Empty();
    for (int i = 0; i < 64; ++i)
        document["obj"].push_back({{"base", json::object()}, {"oid", i}, {"par", i - 1}});
    for (int i = 64; i < 80; ++i)
        document["obj"].push_back({{"base", json::object()}, {"oid", i}, {"par", 63}});
    { auto runtime = Load(document); CHECK(lvgl_runtime_object(runtime.get(), 0, 79)); }
    document["obj"][0]["par"] = 63;
    Reject(document);
    document["obj"][0]["par"] = 99999;
    Reject(document);
}
