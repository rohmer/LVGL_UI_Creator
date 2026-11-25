#include "catch.hpp"
#include <Serialization/Protobuf.h>
#include <Serialization/LVVariable.h>
#include <Serialization/Variable.h>
#include <Serialization/Screen.h>
#include <Serialization/Project.h>
#include <Serialization/ProjectScreens.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/CustomWidgets.h>
#include <limits>
#include <Serialization/ImageAsset.h>
#include <Serialization/FontData.h>
#include <Serialization/ProjectFonts.h>
#include <Serialization/Xml.h>
#include <Serialization/ResourceArena.h>

TEST_CASE("Typed font data preserves raster tables and legacy records with compact transport", "[fonts]")
{
    using namespace Serialization;
    auto font = LVFont::ToJSON(lv_font_montserrat_14);
    REQUIRE(FontData::Matches(font));
    const auto typed = Protobuf::Encode(EntityKind::Font,"font",font);
    const auto legacy = Protobuf::Encode(EntityKind::Font,"font",font,false);
    CHECK(typed.size() < legacy.size() / 4);
    CHECK(Protobuf::Decode(typed).data == font);
    CHECK(Protobuf::Decode(legacy).data == font);
    const auto xml = Xml::Encode(EntityKind::Font,"font",font);
    CHECK(xml.find("<font format_version=\"1\">") != std::string::npos);
    CHECK(xml.find("<binary encoding=\"base64\">") != std::string::npos);
    CHECK(Xml::Decode(xml).data == font);
    const auto encoded = FontData::Encode(font);
    for(size_t prefix=1;prefix<=12;++prefix) {
        auto input=pb_istream_from_buffer(encoded.data(),prefix);
        CHECK_THROWS(FontData::Decode(input));
    }
    auto invalid = encoded;
    REQUIRE(invalid[0] == 8); invalid[1] = 99;
    auto input=pb_istream_from_buffer(invalid.data(),invalid.size());
    CHECK_THROWS(FontData::Decode(input));
    invalid=encoded;invalid.push_back(82);invalid.push_back(0); // duplicate bitmap
    input=pb_istream_from_buffer(invalid.data(),invalid.size());
    CHECK_THROWS(FontData::Decode(input));
    // Nonstandard optional-table presence must use the lossless generic path.
    auto unusual=font;
    unusual["dsc"]["Kern"]={{"Values",json::array()},{"leftCount",0},{"rightCount",0},
        {"leftClasses",json::array()},{"rightClasses",json::array()},{"GlyphIDs",json::array()}};
    CHECK_FALSE(FontData::Matches(unusual));
    CHECK(Protobuf::Decode(Protobuf::Encode(EntityKind::Font,"font",unusual)).data==unusual);
    unusual["dsc"]["Kern"].erase("GlyphIDs");unusual["dsc"]["Kern"]["glyphIDSize"]=0;
    CHECK_FALSE(FontData::Matches(unusual));
    CHECK(Protobuf::Decode(Protobuf::Encode(EntityKind::Font,"font",unusual)).data==unusual);
    // Unknown future metadata must survive via the generic legacy property
    // representation instead of being silently discarded by the typed codec.
    font["futureMetadata"]="preserved";
    CHECK_FALSE(FontData::Matches(font));
    CHECK(Protobuf::Decode(Protobuf::Encode(EntityKind::Font,"font",font)).data==font);
}

TEST_CASE("Font selection creates main binding and preserves independent state fonts", "[fonts]")
{
    using namespace Serialization;
    const auto font = LVFont::ToJSON(lv_font_montserrat_14);
    const json catalog={{{"id","body"},{"name","Body"},{"font",font}}};
    json document={{"base",{{"fontId","body"},{"nativeStyles",{
        {{"selector",LV_STATE_PRESSED},{"values",{{"text_color",123}}}},
        {{"selector",0},{"values",{{"text_color",456},{"text_font",{{"obsolete",true}}}}}}
    }}}}};
    ProjectFonts::References(document,catalog,false);
    const auto& styles=document["base"]["nativeStyles"];
    CHECK(styles[0]["values"]==json{{"text_color",123}});
    CHECK(styles[1]["values"]["text_color"]==456);
    CHECK(styles[1]["values"]["text_font"]==json{{"projectFont","body"}});
    ProjectFonts::References(document,catalog,true);
    CHECK(document["base"]["nativeStyles"][1]["values"]["text_font"]==font);
    ResourceArena arena;
    ResourceScope scope(arena);
    auto& restored=LVFont::FromJSON(font);
    CHECK(LVFont::ToJSON(restored)==font);
    CHECK(&LVFont::FromJSON(font)==&restored);
}

TEST_CASE("Device fonts follow concrete canvases screens and part state styles", "[fonts]")
{
    using namespace Serialization;
    auto font=LVFont::ToJSON(lv_font_montserrat_14);
    auto catalog=json::array();
    for(const auto* id:{"canvas","body","state","inline","unused"}) {
        font["lineHeight"]=font["lineHeight"].get<int>()+1;
        catalog.push_back({{"id",id},{"name",id},{"font",font},{"conversion",{{"path","host-only.ttf"}}}});
    }
    const auto unused=json{{"projectFont","unused"}};
    const auto state=json{{"projectFont","state"}};
    json body={{"base",{{"fontId","body"},{"nativeStyles",json::array({
        {{"selector",0},{"values",{{"text_font",unused}}}},
        {{"selector",(lv_style_selector_t)LV_PART_SELECTED|LV_STATE_PRESSED},{"values",{{"text_font",state}}}}
    })}}},{"text","Body"}};
    body["base"]["design"]["component"]["baseline"]={{"base",{{"fontId","unused"}}}};
    const json inlined={{"base",{{"nativeStyles",json::array({
        {{"selector",0},{"values",{{"text_font",catalog[3]["font"]}}}}
    })}}},{"text","Inline"}};
    json document={{"fonts",catalog},{"screens",json::array({
        {{"canvas",{{"base",{{"fontId","canvas"}}}}},{"obj",json::array({body})}},
        {{"canvas",json::object()},{"obj",json::array({inlined})}}
    })}};
    document["components"]=json::array({{{"records",json::array({{{"base",{{"fontId","unused"}}}}})}}});
    document["sharedStyles"]={{"unused",{{"text_font",unused}}}};
    document["componentLibrary"]={{"font",unused}};
    const auto original=document;
    const auto device=ProjectScreens::Device(document);
    CHECK(document==original);REQUIRE(device["fonts"].size()==4);
    std::set<std::string> ids;for(const auto& record:device["fonts"]){ids.insert(record.at("id"));CHECK_FALSE(record.contains("conversion"));}
    CHECK((ids==std::set<std::string>{"canvas","body","state","inline"}));
    CHECK_FALSE(device.contains("components"));CHECK_FALSE(device.contains("componentLibrary"));CHECK_FALSE(device.contains("sharedStyles"));
    CHECK(device["screens"][0]["obj"][0]["base"]["nativeStyles"][0]["values"]["text_font"]==json{{"projectFont","body"}});
    CHECK(device["screens"][1]["obj"][0]["base"]["nativeStyles"][0]["values"]["text_font"]==json{{"projectFont","inline"}});
    CHECK(Protobuf::Decode(Protobuf::Encode(EntityKind::Project,"project",device)).data["fonts"]==device["fonts"]);
    auto blank=document;blank["screens"]=json::array({{{"obj",json::array()}}});
    CHECK(ProjectScreens::Device(blank)["fonts"].empty());
    auto invalid=document;invalid["screens"][1]["obj"][0]["base"]["fontId"]="missing";
    CHECK_THROWS(ProjectScreens::Device(invalid));
}

TEST_CASE("Structured Protobuf preserves all JSON values and empty containers")
{
    const json record{{"emptyObject", json::object()}, {"emptyArray", json::array()}, {"null", nullptr},
        {"signed", std::numeric_limits<int64_t>::min()}, {"unsigned", std::numeric_limits<uint64_t>::max()},
        {"double", 3.14159}, {"bool", true}, {"unicode", u8"R\u00e9sum\u00e9 \u65e5\u672c\u8a9e"}, {"slash/~", {1, "two", false}},
        {"emptyKey", {{"", "value"}}}, {"bytes", json::binary({0, 1, 127, 255})}};
    const auto bytes = Serialization::Protobuf::Encode(Serialization::EntityKind::Asset, "resource", record);
    REQUIRE(!bytes.empty()); CHECK(bytes.front() != '{');
    const auto decoded = Serialization::Protobuf::Decode(bytes);
    CHECK(decoded.kind == Serialization::EntityKind::Asset); CHECK(decoded.type == "resource"); CHECK(decoded.data == record);
    for (const json value : {json(nullptr), json(""), json::array(), json::object(), json(false), json(-1), json(1.0)})
        CHECK(Serialization::Protobuf::Decode(Serialization::Protobuf::Encode(Serialization::EntityKind::Style, "style", value)).data == value);
}
TEST_CASE("Protobuf rejects truncation unsupported versions and invalid paths")
{
    auto bytes = Serialization::Protobuf::Encode(Serialization::EntityKind::Widget, "test", {{"value", "payload"}});
    bytes.pop_back(); CHECK_THROWS(Serialization::Protobuf::Decode(bytes)); CHECK_THROWS(Serialization::Protobuf::Decode({}));
    auto wrong = Serialization::Protobuf::Encode(Serialization::EntityKind::Widget, "test", {{"value", 1}});
    REQUIRE(wrong[0] == 8); wrong[1] = LVSERIAL_BINARY_VERSION + 1; CHECK_THROWS(Serialization::Protobuf::Decode(wrong));
    CHECK_THROWS(Serialization::Protobuf::Encode(Serialization::EntityKind::Widget, "test", {{std::string(512, 'x'), 1}}));
    CHECK_THROWS(Serialization::Protobuf::Encode(Serialization::EntityKind::Widget, std::string(64, 't'), {}));
    CHECK_THROWS(Serialization::Protobuf::Encode(Serialization::EntityKind::Widget, "test", std::numeric_limits<double>::infinity()));
}
TEST_CASE("Protobuf omits default properties while preserving presence types and overrides")
{
    using namespace Serialization;
    const json document{{"base",{{"name",""},{"hidden",false},{"click",true},{"checked",false},{"disabled",false},
        {"opascale",255},{"drag",false},{"coords",{{"x",0},{"y",0},{"width",120},{"height",40}}},
        {"flags",{{"hidden",false},{"click_focusable",true},{"scrollable",true},{"scroll_elastic",true},
            {"scroll_momentum",true},{"scroll_one",false},{"event_bubble",false},{"checked",false},{"disabled",false}}}}},
        {"bar",{{"mode",0},{"value",0},{"orientation",0},{"startValue",0},{"animT",0},{"sym",false},{"range",{{"min",0},{"max",100}}}}},
        {"custom",{{"hidden",true},{"click",false},{"checked",true},{"text",""},{"max",0},{"min",-10},
            {"unknown",false},{"null",nullptr},{"mode",0.0},{"value",json::array({0,false,"",nullptr,0.0})}}}};
    const auto compact=Protobuf::Encode(EntityKind::Widget,"bar",document);
    const auto full=Protobuf::Encode(EntityKind::Widget,"bar",document,false);
    CHECK(compact.size() < full.size()/2);
    CHECK(Protobuf::Decode(compact).data == document);
    CHECK(Protobuf::Decode(full).data == document);
    CHECK(Protobuf::Decode(compact).data["custom"]["mode"].is_number_float());
    CHECK(Protobuf::Decode(compact).data["custom"]["value"] == document["custom"]["value"]);
    for(const json record : {json{{"hidden",false}},json{{"children",json::array()}},json{{"style",json::object()}},json::object()}) {
        const auto roundTrip=Protobuf::Decode(Protobuf::Encode(EntityKind::Asset,"test",record)).data;
        CHECK(roundTrip==record);CHECK(roundTrip.size()==record.size());
    }
    CHECK(compact[1]==2);CHECK(full[1]==1);
}
TEST_CASE("Every default widget produces a smaller portable Protobuf record")
{
    using namespace Serialization;
    for(const auto& type:ObjectSerializer::SupportedTypes()) {
        DYNAMIC_SECTION(type) {
            auto* object=ObjectSerializer::Create(type);
            const auto record=ObjectSerializer::SerializeObject(object);
            const auto compact=Protobuf::Encode(EntityKind::Widget,type,record);
            const auto full=Protobuf::Encode(EntityKind::Widget,type,record,false);
            CHECK(compact.size()<full.size());CHECK(Protobuf::Decode(compact).data==record);
            auto* copy=ObjectSerializer::FromProtobuf(compact);REQUIRE(copy!=nullptr);
            CHECK(ObjectSerializer::TypeName(copy)==type);
            lv_obj_delete(copy);lv_obj_delete(object);
        }
    }
}
TEST_CASE("LVSerial owns variable defaults bindings histories JSON and Protobuf")
{
    using namespace Serialization;
    for (const auto type : {VariableType::String, VariableType::Integer, VariableType::Boolean, VariableType::Float, VariableType::DateTime})
    {
        VariableData variable;
        variable.name = "sample"; variable.type = type; variable.history = 3; variable.objectId = 42; variable.property = "/bar/value";
        variable.defaultValue = type == VariableType::String ? json("text") : type == VariableType::Boolean ? json(true) :
            type == VariableType::Float ? json(1.25) : type == VariableType::DateTime ? json(int64_t(1700000000)) : json(-5);
        variable.values.push_back(variable.defaultValue);
        const auto record = LVVariable::ToJSON(variable);
        CHECK(LVVariable::ToJSON(LVVariable::FromJSON(record)) == record);
        CHECK(LVVariable::ToJSON(LVVariable::FromProtobuf(LVVariable::ToProtobuf(variable))) == record);
    }
    Variable<int> value("history", 10, -1, "/bar/value", 1);
    value.SetValue(11); value.SetValue(12); CHECK(value.GetValues() == std::vector<int>{12});
    value.SetValues({1, 2, 3}); CHECK(value.GetValues() == std::vector<int>{3});
    CHECK(Variable<int>::FromProtobuf(value.ToProtobuf()).GetValue() == 3);
    CHECK(Variable<int>::FromJSON(value.ToJSON()).ToJSON() == value.ToJSON());
    Variable<int> unlimited("unlimited", 0, 0, "", 0); unlimited.SetValues({1, 2, 3}); unlimited.SetValue(4);
    CHECK(unlimited.GetValues().size() == 4);
    CHECK_THROWS(LVVariable::FromJSON({{"var", {{"dataType", 2}, {"default", "not a bool"}}}}));
}
TEST_CASE("Variable binding lists and C storage metadata normalize without losing legacy records", "[variables]")
{
    using namespace Serialization;
    const json legacy={{"var",{{"name","legacy"},{"dataType",1},{"default",7},{"history",1},{"oid",9},{"property","/slider/value"},{"screen",2}}}};
    const auto old=LVVariable::FromJSON(legacy);REQUIRE(LVVariable::Bindings(old).size()==1);
    CHECK(LVVariable::Bindings(old)[0].screen==2);CHECK(LVVariable::ToJSON(old)==legacy);CHECK(LVVariable::CodeType(old)=="int64_t");
    auto record=legacy;auto& data=record["var"];data["code"]={{"symbol","device_count"},{"cType","uint8_t"},{"note","Application-owned"}};
    data["bindings"]=json::array({{{"oid",3},{"property","/slider/value"},{"label","Dial"}},
        {{"screen",1},{"oid",4},{"property","/text"},{"direction","to-widget"}}});
    data["value"]=json::array({8,9});
    const auto variable=LVVariable::FromJSON(record);const auto normalized=LVVariable::ToJSON(variable);
    CHECK(LVVariable::CodeType(variable)=="uint8_t");CHECK(LVVariable::CodeSymbol(variable)=="device_count");
    CHECK(variable.objectId==3);CHECK(normalized["var"]["screen"]==0);
    CHECK(normalized["var"]["bindings"][0]["direction"]=="two-way");CHECK(normalized["var"]["bindings"][0]["label"]=="Dial");
    CHECK(normalized["var"]["code"]["note"]=="Application-owned");
    CHECK(LVVariable::ToJSON(LVVariable::FromJSON(normalized))==normalized);
    CHECK(LVVariable::ToJSON(LVVariable::FromProtobuf(LVVariable::ToProtobuf(variable)))==normalized);
    data["bindings"]=json::array();const auto unbound=LVVariable::FromJSON(record);
    CHECK(LVVariable::Bindings(unbound).empty());CHECK(unbound.objectId==-1);CHECK(LVVariable::ToJSON(unbound)["var"]["property"]=="");

    for(const auto& invalid:std::vector<json>{nullptr,json::object(),json::array({{{"oid",-1},{"property","value"}}}),
        json::array({{{"oid",0},{"screen",-1},{"property","value"}}}),json::array({{{"oid",0},{"property","value"},{"direction","both"}}}),
        json::array({{{"oid",0},{"property","value"}},{{"oid",0},{"property","/slider/value"}}})}){
        auto bad=record;bad["var"]["bindings"]=invalid;CHECK_THROWS(LVVariable::FromJSON(bad));
    }
    for(const auto* symbol:{"", "1counter", "device.counter", "count;", "switch", "namespace"}){
        auto bad=record;bad["var"]["code"]["symbol"]=symbol;CHECK_THROWS(LVVariable::FromJSON(bad));
    }
    for(const auto& code:std::vector<json>{json("int"),json{{"cType","float"}},json{{"cType","uint8_t"}}}){
        auto bad=record;bad["var"]["code"]=code;bad["var"]["default"]=-1;CHECK_THROWS(LVVariable::FromJSON(bad));
    }
    auto bad=record;bad["var"]["default"]=256;CHECK_THROWS(LVVariable::FromJSON(bad));
    bad=record;bad["var"]["value"]=json::array({256});CHECK_THROWS(LVVariable::FromJSON(bad));
    bad=record;bad["var"]["dataType"]=3;bad["var"]["code"]={{"cType","float"}};bad["var"]["default"]=1e100;CHECK_THROWS(LVVariable::FromJSON(bad));
}
TEST_CASE("The shared variable property catalog enforces widget type direction and writer ownership", "[variables]")
{
    using namespace Serialization;
    VariableData value;value.name="count";
    VariableBinding binding;binding.objectId=0;binding.property="/slider/value";
    CHECK_NOTHROW(LVVariable::ValidateBinding(value,binding,{{"slider",json::object()}}));
    binding.direction=VariableBindingDirection::FromWidget;
    CHECK_NOTHROW(LVVariable::ValidateBinding(value,binding,{{"slider",json::object()}}));
    binding.property="/text";CHECK_THROWS(LVVariable::ValidateBinding(value,binding,{{"text","Caption"}}));
    binding.direction=VariableBindingDirection::TwoWay;CHECK_NOTHROW(LVVariable::ValidateBinding(value,binding,{{"text","Caption"}}));
    value.type=VariableType::String;value.defaultValue="";binding.property="value";
    CHECK_THROWS(LVVariable::ValidateBinding(value,binding,{{"slider",json::object()}}));
    value.type=VariableType::Boolean;value.defaultValue=false;binding.property="checked";
    CHECK_NOTHROW(LVVariable::ValidateBinding(value,binding,{{"switch",json::object()}}));
    binding.direction=VariableBindingDirection::FromWidget;
    CHECK_THROWS(LVVariable::ValidateBinding(value,binding,{{"slider",json::object()}}));
    CHECK_THROWS(LVVariable::ValidateBinding(value,binding,{{"button",{{"toggle",false}}}}));
    CHECK_NOTHROW(LVVariable::ValidateBinding(value,binding,{{"button",{{"toggle",true}}}}));
    CHECK_NOTHROW(LVVariable::ValidateBinding(value,binding,{{"button",json::object()},{"base",{{"flags",{{"checkable",true}}}}}}));
    auto first=LVVariable::ToJSON(value);first["var"]["bindings"]=json::array({{{"oid",0},{"property","checked"},{"direction","to-widget"}}});
    auto second=first;second["var"]["name"]="other";second["var"]["bindings"][0]["property"]="/base/checked";
    CHECK_THROWS(LVVariable::NormalizeList(json::array({first,second})));
    first["var"]["bindings"][0]["direction"]="from-widget";second["var"]["bindings"][0]["direction"]="from-widget";
    CHECK_NOTHROW(LVVariable::NormalizeList(json::array({first,second})));
}
TEST_CASE("Screen CRUD preserves shared binding identities and legacy duplicate semantics", "[variables][screens]")
{
    namespace P=Serialization::ProjectScreens;
    json document={{"screens",json::array({{{"id","a"},{"name","A"},{"obj",json::array({{{"oid",7},{"par",-1}}})},{"canvas",json::object()}},
        {{"id","b"},{"name","B"},{"obj",json::array({{{"oid",9},{"par",-1}}})},{"canvas",json::object()}}})},{"activeScreen",0}};
    document["variables"]=json::array({{{"var",{{"name","shared"},{"bindings",json::array({{{"oid",7},{"property","value"}},{{"screen",1},{"oid",9},{"property","text"},{"direction","to-widget"}}})}}}},
        {{"var",{{"name","legacy"},{"oid",7},{"property","checked"}}}}});
    P::Select(document,0);P::DenseIds(document);
    CHECK(document["variables"][0]["var"]["bindings"][0]["oid"]==0);CHECK(document["variables"][0]["var"]["bindings"][1]["oid"]==0);
    REQUIRE(P::Duplicate(document,0)==2);REQUIRE(document["variables"].size()==3);
    CHECK(document["variables"][0]["var"]["bindings"].size()==3);CHECK(document["variables"][2]["var"]["name"]=="legacy_copy");
    CHECK(document["variables"][0]["var"]["bindings"][2]["screen"]==2);
    P::Move(document,2,0);CHECK(document["variables"][0]["var"]["bindings"][0]["screen"]==1);
    CHECK(document["variables"][0]["var"]["bindings"][1]["screen"]==2);CHECK(document["variables"][0]["var"]["bindings"][2]["screen"]==0);
    CHECK(document["variables"][1]["var"]["screen"]==1);
    P::Remove(document,1);REQUIRE(document["variables"][0]["var"]["bindings"].size()==2);
    CHECK(document["variables"][0]["var"]["bindings"][0]["screen"]==1);CHECK(document["variables"][0]["var"]["screen"]==1);
    CHECK(document["variables"][1]["var"]["oid"]==-1);CHECK(document["variables"][2]["var"]["screen"]==0);
}
TEST_CASE("Screens widgets and full projects round-trip in both formats")
{
    using namespace Serialization;
    auto* screen = lv_obj_create(lv_screen_active()); lv_obj_set_size(screen, 480, 320);
    LVObject::FromJSON({{"name", "Main screen"}}, screen);
    auto* widget = CustomWidgets::Create("textBox", screen); CustomWidgets::Apply(widget, {{"title", "Temperature"}, {"value", 42}});
    auto* widgetCopy = ObjectSerializer::FromProtobuf(ObjectSerializer::ToProtobuf(widget));
    REQUIRE(widgetCopy != nullptr); CHECK(ObjectSerializer::TypeName(widgetCopy) == std::string("textBox"));
    CHECK(CustomWidgets::ToJSON(widgetCopy)["textBox"]["value"] == 42);
    auto* screenCopy = Screen::FromProtobuf(Screen::ToProtobuf(screen)); REQUIRE(screenCopy != nullptr);
    CHECK(lv_obj_get_width(screenCopy) == 480); CHECK(Screen::ToJSON(screenCopy)["base"]["name"] == "Main screen");
    const json variables = json::array({{{"var", {{"name", "temperature"}, {"dataType", 1}, {"default", 42}, {"oid", 0},
        {"property", "/textBox/value"}, {"history", 3}, {"value", {40, 41, 42}}}}}});
    const auto document = Project::ToJSON({widget}, screen, variables);
    const auto binary = Project::ToProtobuf({widget}, screen, variables);
    CHECK(binary.size() < Protobuf::Encode(EntityKind::Project,"project",document,false).size());
    CHECK(Protobuf::Decode(binary).data == document);
    auto restored = Project::FromProtobuf(binary); REQUIRE(restored.objects.size() == 1);
    CHECK(restored.screen["width"] == 480); CHECK(restored.variables == variables);
    CHECK(CustomWidgets::ToJSON(restored.objects[0])["textBox"]["value"] == 42);
    CHECK_THROWS(ObjectSerializer::FromProtobuf(Screen::ToProtobuf(screen)));
    lv_obj_delete(restored.objects[0]); lv_obj_delete(widgetCopy); lv_obj_delete(screenCopy); lv_obj_delete(screen);
}


TEST_CASE("GIF PNG and JPG imports are portable in JSON and Protobuf")
{
    using namespace Serialization;
    for (const auto* file : {"sample.gif", "sample.png", "sample.jpg"})
    {
        CAPTURE(file);
        auto* image = lv_image_create(lv_screen_active());
        LVObject::FromJSON(json::object(), image);
        const auto path = std::filesystem::path("../../Fixtures/Images") / file;
        ImageAsset::LoadFromFile(image, path);
        const auto record = ObjectSerializer::SerializeObject(image);
        REQUIRE(record["img"]["asset"]["sourceData"].is_string());
        CHECK(record["img"]["asset"]["width"] == 4);
        CHECK(record["img"]["asset"]["height"] == 3);
        CHECK_FALSE(record["img"].contains("deviceStorage"));
        if (std::string(file) == "sample.gif") CHECK(record["img"]["asset"]["frames"] == 2);
        auto* copy = ObjectSerializer::DeserializeObject(record);
        auto* binaryCopy = ObjectSerializer::FromProtobuf(ObjectSerializer::ToProtobuf(image));
        REQUIRE(copy != nullptr); REQUIRE(binaryCopy != nullptr);
        CHECK(ObjectSerializer::SerializeObject(copy)["img"]["asset"] == record["img"]["asset"]);
        CHECK(ObjectSerializer::SerializeObject(binaryCopy)["img"]["asset"] == record["img"]["asset"]);
        CHECK_THROWS(ImageAsset::LoadFromFile(image, "unsupported.bmp"));
        CHECK(ObjectSerializer::SerializeObject(image)["img"]["asset"] == record["img"]["asset"]);
        lv_obj_delete(image); lv_obj_delete(copy); lv_obj_delete(binaryCopy);
    }
}
