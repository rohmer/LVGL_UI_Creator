#include "catch.hpp"
#if __has_include(<RuntimeCompat.h>)
#include <RuntimeCompat.h>
#endif
#include "lvgl_runtime.h"
#include "CCodeGenerator.h"
#include <Serialization/InteractionActions.h>
#include <Serialization/LVVariable.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/ObjectUserData.h>
#include <Serialization/Page.h>
#include <Serialization/Protobuf.h>
#include <cstring>
#include <memory>
using json=nlohmann::json;
namespace {
json Widget(int oid,const char* kind,int x,int y,int width,int height) {return {{"oid",oid},{"par",-1},{"base",{{"coords",{{"x",x},{"y",y},{"width",width},{"height",height}}}}},{kind,json::object()}};}
json Document(){auto button=Widget(7,"button",10,10,100,50),entry=Widget(9,"textarea",10,70,150,50),panel=Widget(10,"text",180,70,80,50);entry["textarea"]["text"]="";panel["text"]="Result";button["base"]["events"]=json::array({{{"event","clicked"},{"action","validate"},{"targetOid",9},{"required",true},{"min",10},{"max",20}},{{"event","clicked"},{"action","increment_variable"},{"variable","count"},{"value",2},{"condition",{{"variable","enabled"},{"op","truthy"}}}},{{"event","clicked"},{"action","hide"},{"targetOid",10}}});Serialization::VariableData count;count.name="count";count.history=5;Serialization::VariableData enabled;enabled.name="enabled";enabled.type=Serialization::VariableType::Boolean;enabled.defaultValue=true;return {{"obj",json::array({button,entry,panel})},{"canvas",{{"width",320},{"height",240}}},{"variables",json::array({Serialization::LVVariable::ToJSON(count),Serialization::LVVariable::ToJSON(enabled)})}};}
}
TEST_CASE("Portable interactions validate input before typed variable and widget actions", "[runtime][interactions]") {
    auto document=Document();auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",document);lvgl_runtime_t* raw=nullptr;char error[256]{};REQUIRE(lvgl_runtime_load_memory(bytes.data(),bytes.size(),nullptr,&raw,error,sizeof error));std::unique_ptr<lvgl_runtime_t,decltype(&lvgl_runtime_destroy)> runtime(raw,lvgl_runtime_destroy);
    auto* button=static_cast<lv_obj_t*>(lvgl_runtime_object(raw,0,7));auto* entry=static_cast<lv_obj_t*>(lvgl_runtime_object(raw,0,9));auto* panel=static_cast<lv_obj_t*>(lvgl_runtime_object(raw,0,10));REQUIRE(button);REQUIRE(entry);REQUIRE(panel);
    lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);lvgl_runtime_value_t value{};REQUIRE(lvgl_runtime_get_variable(raw,"count",&value));CHECK(value.integer_value==0);CHECK(lv_obj_has_state(entry,LV_STATE_USER_4));CHECK_FALSE(lv_obj_is_hidden(panel));
    lv_textarea_set_text(entry,"17");lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);REQUIRE(lvgl_runtime_get_variable(raw,"count",&value));CHECK(value.integer_value==2);CHECK_FALSE(lv_obj_has_state(entry,LV_STATE_USER_4));CHECK(lv_obj_is_hidden(panel));
    lvgl_runtime_value_t enabled{};enabled.type=LVGL_RUNTIME_BOOLEAN;enabled.boolean_value=false;REQUIRE(lvgl_runtime_set_variable(raw,"enabled",&enabled));lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);REQUIRE(lvgl_runtime_get_variable(raw,"count",&value));CHECK(value.integer_value==2);
    lv_obj_delete(panel);lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);CHECK(std::strstr(lvgl_runtime_error(raw),"missing widget")!=nullptr);lv_obj_delete(button);
}
TEST_CASE("Authored action targets follow objects through dense save IDs and deletion", "[runtime][interactions]") {
    auto record=Widget(50,"button",0,0,80,44);record["base"]["events"]=json::array({{{"action","toggle"},{"targetOid",90}}});auto target=Widget(90,"text",0,60,80,44);auto objects=Serialization::Page::FromJSON({{"obj",json::array({record,target})}});REQUIRE(objects.size()==2);
    static_cast<ObjectUserData*>(lv_obj_get_user_data(objects[0]))->objectID=1000;static_cast<ObjectUserData*>(lv_obj_get_user_data(objects[1]))->objectID=2000;
    auto saved=Serialization::Page::ToJSON({objects[1],objects[0]});CHECK(saved["obj"][1]["base"]["events"][0]["targetOid"]==0);
    auto* source=objects[0];lv_obj_delete(objects[1]);saved=Serialization::Page::ToJSON({source});CHECK(saved["obj"][0]["base"]["events"][0]["targetOid"]==-2);lv_obj_delete(source);
}
TEST_CASE("Runtime visibility actions preserve unrelated object flags", "[runtime][interactions]") {
    auto document = Document();
    document["obj"][0]["base"]["events"] = json::array({
        {{"event", "clicked"}, {"action", "toggle"}, {"targetOid", 10}},
        {{"event", "ready"}, {"action", "hide"}, {"targetOid", 10}},
        {{"event", "value_changed"}, {"action", "show"}, {"targetOid", 10}}
    });
    auto bytes = Serialization::Protobuf::Encode(Serialization::EntityKind::Project, "project", document);
    lvgl_runtime_t* raw = nullptr;
    char error[256]{};
    REQUIRE(lvgl_runtime_load_memory(bytes.data(), bytes.size(), nullptr, &raw, error, sizeof error));
    std::unique_ptr<lvgl_runtime_t, decltype(&lvgl_runtime_destroy)> runtime(raw, lvgl_runtime_destroy);
    auto* source = static_cast<lv_obj_t*>(lvgl_runtime_object(raw, 0, 7));
    auto* panel = static_cast<lv_obj_t*>(lvgl_runtime_object(raw, 0, 10));
    REQUIRE(source);
    REQUIRE(panel);
    lv_obj_set_clickable(panel, true);
    lv_obj_set_scrollable(panel, false);
    auto check = [&](bool hidden) {
        CHECK(lv_obj_is_hidden(panel) == hidden);
        CHECK(lv_obj_is_clickable(panel));
        CHECK_FALSE(lv_obj_is_scrollable(panel));
        CHECK(std::strlen(lvgl_runtime_error(raw)) == 0);
    };
    lv_obj_send_event(source, LV_EVENT_CLICKED, nullptr); check(true);
    lv_obj_send_event(source, LV_EVENT_CLICKED, nullptr); check(false);
    lv_obj_send_event(source, LV_EVENT_READY, nullptr); check(true);
    lv_obj_send_event(source, LV_EVENT_READY, nullptr); check(true);
    lv_obj_send_event(source, LV_EVENT_VALUE_CHANGED, nullptr); check(false);
    lv_obj_send_event(source, LV_EVENT_VALUE_CHANGED, nullptr); check(false);
}
TEST_CASE("Action schema rejects invalid bounds and conditions", "[runtime][interactions]") {
    CHECK_THROWS(Serialization::InteractionActions::Normalize(json::array({{{"action","validate"},{"min",20},{"max",10}}})));
    CHECK_THROWS(Serialization::InteractionActions::Normalize(json::array({{{"action","navigate"},{"screen",-1}}})));
    CHECK_THROWS(Serialization::InteractionActions::Normalize(json::array({{{"action","hide"},{"condition",{{"variable","count"},{"op","javascript"}}}}})));
}
TEST_CASE("C export emits portable interactions for each supported target", "[targets][interactions]") {
    for(const char* target:{"9.6","9.5","9.3","9.2","9.1","8.3"}){CCodeGenerator generator;generator.SetTargetVersion(target);REQUIRE(generator.LoadProject(Document()));json summary;bool okay=generator.Preflight(summary);INFO(generator.GetLastError());CHECK(okay);}
    auto document=Document();document["obj"][0]["base"]["events"][2]["targetOid"]=999;CCodeGenerator generator;REQUIRE(generator.LoadProject(document));json summary;CHECK_FALSE(generator.Preflight(summary));CHECK(generator.GetLastError().find("missing widget")!=std::string::npos);
}
