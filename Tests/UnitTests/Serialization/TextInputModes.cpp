#include "catch.hpp"
#include <Serialization/BasicWidgets.h>
#include <Serialization/CustomWidgets.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Protobuf.h>
#include <Serialization/Xml.h>
#include <utility>
#include <vector>

#if LVGL_VERSION_MAJOR >= 9
TEST_CASE("Text input modes preserve unrestricted defaults and round trip through Protobuf and XML", "[input-modes]")
{
    using namespace Serialization;
    for (const auto* type : {"textarea", "textBox"})
    {
        CAPTURE(type);
        const bool composite = std::string(type) == "textBox";
        for (const auto& sample : std::vector<std::pair<std::string, std::string>>{
            {"alphanumeric", u8"aB \u00e0\u0416 1.!"}, {"uppercase", u8"AB \u00c0\u0416 1.!"},
            {"lowercase", u8"ab \u00e0\u0436 1.!"}, {"numeric", "1."}})
        {
            CAPTURE(sample.first);
            const json document{{"base", json::object()}, {type, {{"text", u8"aB \u00e0\u0416 1.!"},
                {"numeric", false}, {"inputMode", sample.first}}}};
            auto* object = ObjectSerializer::DeserializeObject(document);
            REQUIRE(object);
            auto* entry = composite ? CustomWidgets::Part(object, "Entry") : object;
            CHECK(std::string(lv_textarea_get_text(entry)) == sample.second);
            CHECK(lv_advanced_keyboard_get_textarea_input_mode(entry) == TextInputMode::Parse(sample.first));
            const auto record = ObjectSerializer::SerializeObject(object);
            CHECK(record.at(type).value("inputMode", std::string("alphanumeric")) == sample.first);
            for (const auto& decoded : {Protobuf::Decode(Protobuf::Encode(EntityKind::Widget, type, record)).data,
                 Xml::Decode(Xml::Encode(EntityKind::Widget, type, record)).data})
            {
                auto* restored = ObjectSerializer::DeserializeObject(decoded);
                REQUIRE(restored);
                auto* restoredEntry = composite ? CustomWidgets::Part(restored, "Entry") : restored;
                CHECK(std::string(lv_textarea_get_text(restoredEntry)) == sample.second);
                CHECK(lv_advanced_keyboard_get_textarea_input_mode(restoredEntry) == TextInputMode::Parse(sample.first));
                lv_textarea_set_cursor_pos(restoredEntry, LV_TEXTAREA_CURSOR_LAST);
                lv_textarea_add_text(restoredEntry, sample.first == "numeric" ? "a2.3b" : "qR");
                const auto suffix = sample.first == "uppercase" ? "QR" : sample.first == "lowercase" ? "qr" :
                    sample.first == "numeric" ? "23" : "qR";
                CHECK(std::string(lv_textarea_get_text(restoredEntry)) == sample.second + suffix);
                lv_obj_delete(restored);
            }
            lv_obj_delete(object);
        }
    }
}

TEST_CASE("Text box input mode combines with character filters and maximum length", "[input-modes]")
{
    using namespace Serialization;
    for (const auto* type : {"textarea", "textBox"})
    {
        CAPTURE(type);
        const bool composite = std::string(type) == "textBox";
        const json document{{"base", json::object()}, {type, {{"text", "1"}, {"numeric", false},
            {"inputMode", "numeric"}, {"maxLength", 4}, {composite ? "acceptedChars" : "accepted", "123."}}}};
        auto* object = ObjectSerializer::DeserializeObject(document);
        REQUIRE(object);
        auto* entry = composite ? CustomWidgets::Part(object, "Entry") : object;
        lv_textarea_set_cursor_pos(entry, LV_TEXTAREA_CURSOR_LAST);
        lv_textarea_add_text(entry, "9a2.3.1");
        CHECK(std::string(lv_textarea_get_text(entry)) == "12.3");
        lv_textarea_set_text(entry, "a1b2..3x");
        CHECK(std::string(lv_textarea_get_text(entry)) == "12.3");
        const auto record = ObjectSerializer::SerializeObject(object).at(type);
        CHECK(record.at(composite ? "acceptedChars" : "accepted") == "123.");
        CHECK(record.at("maxLength") == 4);
        if (composite) CHECK_THROWS(CustomWidgets::Apply(object, {{"inputMode", "unknown"}}));
        else CHECK_THROWS(BasicWidgets::Apply(object, {{"inputMode", "unknown"}}));
        CHECK(lv_advanced_keyboard_get_textarea_input_mode(entry) == LV_ADVANCED_KEYBOARD_INPUT_NUMERIC);
        lv_obj_delete(object);
    }
}

TEST_CASE("Programmatic case normalization applies before accepted characters and text length", "[input-modes]")
{
    using namespace Serialization;
    for (const auto* type : {"textarea", "textBox"})
    {
        CAPTURE(type);
        const bool composite = std::string(type) == "textBox";
        auto* object = ObjectSerializer::DeserializeObject({{"base", json::object()},
            {type, {{"text", u8"a\u00dfb.\u00e9"}, {"numeric", false}, {"inputMode", "uppercase"},
                {"maxLength", 4}, {composite ? "acceptedChars" : "accepted", "ABS."}}}});
        REQUIRE(object);
        auto* entry = composite ? CustomWidgets::Part(object, "Entry") : object;
        CHECK(std::string(lv_textarea_get_text(entry)) == "ASSB");
        lv_textarea_set_text(entry, u8"b\u00dfa");
        CHECK(std::string(lv_textarea_get_text(entry)) == "BSSA");
        lv_textarea_set_password_mode(entry, true);
        lv_textarea_set_text(entry, "aBbA");
        CHECK(std::string(lv_textarea_get_text(entry)) == "ABBA");
        lv_obj_delete(object);
    }
}

TEST_CASE("Legacy text boxes remain unrestricted and changing mode normalizes saved text", "[input-modes]")
{
    using namespace Serialization;
    auto* native = BasicWidgets::FromJSON({{"base", json::object()}, {"textarea", {{"text", "Abc ! 123"}}}});
    REQUIRE(native);
    CHECK_FALSE(BasicWidgets::ToJSON(native).at("textarea").contains("inputMode"));
    BasicWidgets::Apply(native, {{"inputMode", "uppercase"}, {"text", u8"\u00c9t\u00e9 \u042f"}});
    CHECK(BasicWidgets::ToJSON(native).at("textarea").at("text") == u8"\u00c9T\u00c9 \u042f");
    BasicWidgets::Apply(native, {{"inputMode", "lowercase"}, {"text", u8"\u00c9T\u00c9 \u042f"}});
    CHECK(BasicWidgets::ToJSON(native).at("textarea").at("text") == u8"\u00e9t\u00e9 \u044f");
    auto* custom = CustomWidgets::Create("textBox");
    CustomWidgets::Apply(custom, {{"numeric", false}, {"text", "a1.2.3b"}});
    CHECK(CustomWidgets::ToJSON(custom).at("textBox").at("text") == "a1.2.3b");
    CustomWidgets::Apply(custom, {{"inputMode", "numeric"}});
    CHECK(CustomWidgets::ToJSON(custom).at("textBox").at("text") == "1.23");
    CHECK(CustomWidgets::ToJSON(custom).at("textBox").at("inputMode") == "numeric");
    lv_obj_delete(native);
    lv_obj_delete(custom);
}

TEST_CASE("Numeric input preserves decimal text independently of the default integer stepper", "[input-modes]")
{
    using namespace Serialization;
    for (const char* text : {"7.3", "7.", ".3", ".", ""})
    {
        CAPTURE(text);
        auto* object = CustomWidgets::FromJSON({{"base", json::object()},
            {"textBox", {{"inputMode", "numeric"}, {"text", text}, {"value", 7}}}});
        REQUIRE(object);
        CHECK(std::string(lv_textarea_get_text(CustomWidgets::Part(object, "Entry"))) == text);
        const auto record = ObjectSerializer::SerializeObject(object);
        CHECK(record.at("textBox").at("numeric") == true);
        CHECK(record.at("textBox").at("value") == 7);
        for (const auto& decoded : {record, Protobuf::Decode(Protobuf::Encode(EntityKind::Widget, "textBox", record)).data,
            Xml::Decode(Xml::Encode(EntityKind::Widget, "textBox", record)).data})
        {
            auto* restored = CustomWidgets::FromJSON(decoded);
            REQUIRE(restored);
            CHECK(std::string(lv_textarea_get_text(CustomWidgets::Part(restored, "Entry"))) == text);
            CHECK(CustomWidgets::ToJSON(restored).at("textBox").at("value") == 7);
            lv_obj_delete(restored);
        }
        lv_obj_delete(object);
    }
}

TEST_CASE("Numeric integer stepper uses a nonnegative effective range without changing authored bounds", "[input-modes]")
{
    using namespace Serialization;
    auto* object = CustomWidgets::Create("textBox");
    CustomWidgets::Apply(object, {{"inputMode", "numeric"}, {"min", -10}, {"max", 10}, {"value", -3}});
    auto record = CustomWidgets::ToJSON(object).at("textBox");
    CHECK(record.at("min") == -10);
    CHECK(record.at("value") == 0);
    CHECK(record.at("text") == "0");
    lv_obj_send_event(CustomWidgets::Part(object, "Minus"), LV_EVENT_CLICKED, nullptr);
    CHECK(CustomWidgets::ToJSON(object).at("textBox").at("text") == "0");
    lv_obj_send_event(CustomWidgets::Part(object, "Plus"), LV_EVENT_CLICKED, nullptr);
    CHECK(CustomWidgets::ToJSON(object).at("textBox").at("text") == "1");
    CustomWidgets::Apply(object, {{"min", -8}, {"max", -1}, {"value", -4}});
    for (const char* button : {"Minus", "Plus"})
        lv_obj_send_event(CustomWidgets::Part(object, button), LV_EVENT_CLICKED, nullptr);
    record = CustomWidgets::ToJSON(object).at("textBox");
    CHECK(record.at("min") == -8);
    CHECK(record.at("max") == -1);
    CHECK(record.at("value") == 0);
    CHECK(record.at("text") == "0");
    CustomWidgets::Apply(object, {{"min", -10}, {"max", 10}, {"text", "a3b"}});
    record = CustomWidgets::ToJSON(object).at("textBox");
    CHECK(record.at("text") == "3");
    CHECK(record.at("value") == 3);
    lv_textarea_set_text(CustomWidgets::Part(object, "Entry"), "a4b");
    CHECK(CustomWidgets::ToJSON(object).at("textBox").at("value") == 4);
    lv_obj_delete(object);
}
#endif
