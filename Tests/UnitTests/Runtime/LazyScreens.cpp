#include "catch.hpp"
#if __has_include(<RuntimeCompat.h>)
#include <RuntimeCompat.h>
#endif
#include "lvgl_runtime.h"
#include <Serialization/Protobuf.h>
#include <Serialization/ProjectTries.h>
#include <Serialization/RuntimeFeatures.h>
#if LVSERIAL_USE_ADVANCED_KEYBOARD
#include <lv_advanced_keyboard_prediction.h>
#endif
#include <cstring>
#include <memory>

namespace {
using json=nlohmann::json;
using Runtime=std::unique_ptr<lvgl_runtime_t,decltype(&lvgl_runtime_destroy)>;
json Widget(int id,const char* kind,json value) {
    return {{"oid",id},{"par",-1},{"base",{{"coords",{{"x",10},{"y",10},{"width",150},{"height",40}}}}},{kind,std::move(value)}};
}
json Project() {
    auto source=Widget(7,"button",json::object());
    source["base"]["events"]=json::array({{{"event","clicked"},{"action","increment_variable"},{"variable","count"},{"value",2}},
        {{"event","clicked"},{"action","navigate"},{"screen",1}}});
    return {{"runtimeTarget",lvgl_runtime_backend()},{"activeScreen",0},
        {"variables",json::array({{{"var",{{"name","count"},{"dataType",1},{"default",3},{"history",5},{"screen",1},{"oid",20},{"property","text"}}}}})},
        {"screens",json::array({
            {{"name","First"},{"canvas",{{"width",320},{"height",240}}},{"obj",json::array({source,Widget(9,"textarea",{{"text","remember"}})})}},
            {{"name","Second"},{"canvas",{{"width",320},{"height",240}}},{"obj",json::array({Widget(20,"text","0")})}},
            {{"name","Third"},{"canvas",{{"width",320},{"height",240}}},{"obj",json::array()}}
        })}};
}
Runtime Load(json document,uint32_t flags,const lvgl_runtime_options_t* options=nullptr) {
    auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",document);
    lvgl_runtime_t* value=nullptr;char error[512]{};
    const auto ok=lvgl_runtime_load_memory_ex(bytes.data(),bytes.size(),options,flags,&value,error,sizeof error);
    INFO(error);REQUIRE(ok);return Runtime(value,lvgl_runtime_destroy);
}
void Timers(){for(int i=0;i<5;++i){lv_tick_inc(25);lv_timer_handler();}}
}
TEST_CASE("Lazy screens allocate on demand and preserve variables and widget state", "[runtime][lazy]") {
    auto* previous=lv_screen_active();auto runtime=Load(Project(),LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE);
    CHECK(lvgl_runtime_screen_count(runtime.get())==3);CHECK(lvgl_runtime_loaded_screen_count(runtime.get())==1);
    CHECK(lvgl_runtime_active_screen(runtime.get())==0);CHECK(lvgl_runtime_screen(runtime.get(),1)==nullptr);
    auto* entry=lvgl_runtime_object(runtime.get(),0,9);REQUIRE(entry);lv_textarea_set_text(entry,"changed while running");
    lv_textarea_set_cursor_pos(entry,7);lv_textarea_set_max_length(entry,64);
    CHECK(std::strcmp(lv_textarea_get_text(entry),"changed while running")==0);
    lv_obj_send_event(lvgl_runtime_object(runtime.get(),0,7),LV_EVENT_CLICKED,nullptr);
    CHECK(lvgl_runtime_active_screen(runtime.get())==1);CHECK(lvgl_runtime_back_count(runtime.get())==1);
    CHECK(std::strcmp(lv_label_get_text(lvgl_runtime_object(runtime.get(),1,20)),"5")==0);
    Timers();CHECK(lvgl_runtime_loaded_screen_count(runtime.get())==1);CHECK(lvgl_runtime_screen(runtime.get(),0)==nullptr);
    lvgl_runtime_value_t count{};REQUIRE(lvgl_runtime_get_variable(runtime.get(),"count",&count));CHECK(count.integer_value==5);
    count.integer_value=17;REQUIRE(lvgl_runtime_set_variable(runtime.get(),"count",&count));
    REQUIRE(lvgl_runtime_back(runtime.get()));CHECK(lvgl_runtime_active_screen(runtime.get())==0);
    INFO(lv_textarea_get_text(lvgl_runtime_object(runtime.get(),0,9)));
    CHECK(std::strcmp(lv_textarea_get_text(lvgl_runtime_object(runtime.get(),0,9)),"changed while running")==0);
    CHECK(lv_textarea_get_cursor_pos(lvgl_runtime_object(runtime.get(),0,9))==7);
    CHECK(lv_textarea_get_max_length(lvgl_runtime_object(runtime.get(),0,9))==64);
    Timers();CHECK(lvgl_runtime_loaded_screen_count(runtime.get())==1);
    REQUIRE(lvgl_runtime_show_screen(runtime.get(),1));CHECK(std::strcmp(lv_label_get_text(lvgl_runtime_object(runtime.get(),1,20)),"17")==0);
    Timers();lvgl_runtime_stats_t stats{};REQUIRE(lvgl_runtime_get_stats(runtime.get(),&stats));
    CHECK(stats.input_bytes>0);CHECK(stats.loaded_screens==1);CHECK(stats.screen_loads>=4);CHECK(stats.screen_unloads>=3);
    CHECK_FALSE(lvgl_runtime_unload_screen(runtime.get(),1));CHECK(std::strlen(lvgl_runtime_error(runtime.get()))>0);
    CHECK_FALSE(lvgl_runtime_ensure_screen(runtime.get(),99));runtime.reset();CHECK(lv_screen_active()==previous);
}
#if LV_USE_SLIDER && LV_USE_LABEL
TEST_CASE("A shared variable reconnects individual bindings across lazy screen unloads", "[runtime][lazy][variables]") {
    auto document=Project();document["screens"][0]["obj"].push_back(Widget(40,"slider",{{"min",0},{"max",200},{"value",0}}));
    auto& variable=document["variables"][0]["var"];
    variable["bindings"]=json::array({{{"screen",0},{"oid",40},{"property","value"}},
        {{"screen",1},{"oid",20},{"property","text"},{"direction","to-widget"}}});
    auto runtime=Load(document,LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE);
    CHECK(lvgl_runtime_screen(runtime.get(),1)==nullptr);CHECK(lv_slider_get_value(lvgl_runtime_object(runtime.get(),0,40))==3);
    for(int cycle=0;cycle<3;++cycle){
        auto* source=lvgl_runtime_object(runtime.get(),0,40);REQUIRE(source);const auto before=lvgl_runtime_history_count(runtime.get(),"count");
        lv_slider_set_value(source,10+cycle,LV_ANIM_OFF);lv_obj_send_event(source,LV_EVENT_VALUE_CHANGED,nullptr);
        CHECK(lvgl_runtime_history_count(runtime.get(),"count")==before+1);
        REQUIRE(lvgl_runtime_show_screen(runtime.get(),1));Timers();CHECK(lvgl_runtime_screen(runtime.get(),0)==nullptr);
        CHECK(std::string(lv_label_get_text(lvgl_runtime_object(runtime.get(),1,20)))==std::to_string(10+cycle));
        lvgl_runtime_value_t value{};value.type=LVGL_RUNTIME_INTEGER;value.integer_value=30+cycle;
        REQUIRE(lvgl_runtime_set_variable(runtime.get(),"count",&value));
        REQUIRE(lvgl_runtime_back(runtime.get()));Timers();
        CHECK(lv_slider_get_value(lvgl_runtime_object(runtime.get(),0,40))==30+cycle);
    }
}
#endif
TEST_CASE("Lazy initial-screen suppression and eager defaults remain compatible", "[runtime][lazy]") {
    auto* previous=lv_screen_active();lvgl_runtime_options_t options{lv_display_get_default(),0,false};
    auto lazy=Load(Project(),LVGL_RUNTIME_LAZY_SCREENS,&options);
    CHECK(lvgl_runtime_loaded_screen_count(lazy.get())==0);CHECK(lvgl_runtime_active_screen(lazy.get())==SIZE_MAX);
    CHECK(lv_screen_active()==previous);REQUIRE(lvgl_runtime_ensure_screen(lazy.get(),1));CHECK(lv_screen_active()==previous);
    REQUIRE(lvgl_runtime_show_screen(lazy.get(),1));CHECK(lvgl_runtime_active_screen(lazy.get())==1);
    lazy.reset();CHECK(lv_screen_active()==previous);
    auto bytes=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",Project());
    lvgl_runtime_t* eager=nullptr;char error[256]{};REQUIRE(lvgl_runtime_load_memory(bytes.data(),bytes.size(),nullptr,&eager,error,sizeof error));
    CHECK(lvgl_runtime_loaded_screen_count(eager)==3);lvgl_runtime_destroy(eager);
}
TEST_CASE("Repeated lazy navigation keeps active action and animation ownership bounded", "[runtime][lazy]") {
    auto runtime=Load(Project(),LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE);
    for(int cycle=0;cycle<12;++cycle) {
        REQUIRE(lvgl_runtime_show_screen(runtime.get(),size_t(cycle%3)));Timers();CHECK(lvgl_runtime_loaded_screen_count(runtime.get())==1);
    }
    CHECK_FALSE(lvgl_runtime_show_screen_animated(runtime.get(),0,LVGL_RUNTIME_TRANSITION_FADE,2001));
    REQUIRE(lvgl_runtime_show_screen_animated(runtime.get(),0,LVGL_RUNTIME_TRANSITION_FADE,80));
    CHECK_FALSE(lvgl_runtime_unload_screen(runtime.get(),0));
    lv_tick_inc(25);lv_timer_handler();REQUIRE(lvgl_runtime_screen(runtime.get(),0));
    Timers();Timers();CHECK(lvgl_runtime_active_screen(runtime.get())==0);CHECK(lvgl_runtime_loaded_screen_count(runtime.get())==1);
}
TEST_CASE("Pending screen animations can be interrupted and destroyed safely", "[runtime][lazy][transitions]") {
    auto* previous=lv_screen_active();
    for(int cycle=0;cycle<6;++cycle) {
        auto runtime=Load(Project(),LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE);
        REQUIRE(lvgl_runtime_show_screen_animated(runtime.get(),1,LVGL_RUNTIME_TRANSITION_LEFT,80));
        CHECK_FALSE(lvgl_runtime_unload_screen(runtime.get(),1));
        // Before its first timer tick, interrupt a pending target with a lazy
        // screen that still needs construction, then destroy during transition.
        REQUIRE(lvgl_runtime_show_screen_animated(runtime.get(),2,LVGL_RUNTIME_TRANSITION_FADE,80));
        runtime.reset();Timers();CHECK(lv_screen_active()==previous);
    }
}
#if LV_USE_SLIDER && LV_USE_SWITCH && LV_USE_CHECKBOX && LV_USE_DROPDOWN && LV_USE_ROLLER && LV_USE_SPINBOX && LV_USE_ARC && LV_USE_BAR
TEST_CASE("Lazy snapshots retain unbound interactive values and flags", "[runtime][lazy][state]") {
    auto document=Project();auto& widgets=document["screens"][0]["obj"];
    widgets.push_back(Widget(40,"slider",{{"min",-100},{"max",200},{"mode",int(LV_SLIDER_MODE_RANGE)}}));
    widgets.push_back(Widget(41,"switch",{{"checked",false}}));
    widgets.push_back(Widget(42,"dropdown",{{"options","One\nTwo\nThree"}}));
    widgets.push_back(Widget(43,"roller",{{"options","One\nTwo\nThree"}}));
    widgets.push_back(Widget(44,"spinbox",{{"min",-100},{"max",200},{"value",0}}));
    widgets.push_back(Widget(45,"cb",{{"text","Choice"},{"checked",false},{"inact",false}}));
    widgets.push_back(Widget(46,"arc",{{"range",{{"min",-50},{"max",100}}},{"value",0}}));
    widgets.push_back(Widget(47,"bar",{{"range",{{"min",-50},{"max",100}}},{"value",0}}));
    auto runtime=Load(document,LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE);
    auto object=[&](int id){return lvgl_runtime_object(runtime.get(),0,id);};
    lv_slider_set_range(object(40),-200,400);lv_slider_set_value(object(40),123,LV_ANIM_OFF);lv_slider_set_left_value(object(40),-66,LV_ANIM_OFF);
    lv_obj_add_state(object(41),LV_STATE_CHECKED);lv_dropdown_set_selected(object(42),2);lv_roller_set_selected(object(43),1,LV_ANIM_OFF);
    lv_spinbox_set_value(object(44),-17);lv_obj_add_state(object(45),LV_STATE_CHECKED);lv_obj_add_state(object(45),LV_STATE_DISABLED);
    lv_arc_set_value(object(46),17);lv_bar_set_value(object(47),32,LV_ANIM_OFF);
    lv_obj_add_flag(object(7),LV_OBJ_FLAG_HIDDEN);lv_obj_add_state(object(7),LV_STATE_DISABLED);
    REQUIRE(lvgl_runtime_show_screen(runtime.get(),1));Timers();CHECK(lvgl_runtime_screen(runtime.get(),0)==nullptr);
    REQUIRE(lvgl_runtime_back(runtime.get()));Timers();
    CHECK(lv_slider_get_value(object(40))==123);CHECK(lv_slider_get_left_value(object(40))==-66);
    CHECK(lv_slider_get_min_value(object(40))==-200);CHECK(lv_slider_get_max_value(object(40))==400);
    CHECK(lv_obj_has_state(object(41),LV_STATE_CHECKED));CHECK(lv_dropdown_get_selected(object(42))==2);CHECK(lv_roller_get_selected(object(43))==1);
    CHECK(lv_spinbox_get_value(object(44))==-17);CHECK(lv_obj_has_state(object(45),LV_STATE_CHECKED));CHECK(lv_obj_has_state(object(45),LV_STATE_DISABLED));
    CHECK(lv_arc_get_value(object(46))==17);CHECK(lv_bar_get_value(object(47))==32);
    CHECK(lv_obj_has_flag(object(7),LV_OBJ_FLAG_HIDDEN));CHECK(lv_obj_has_state(object(7),LV_STATE_DISABLED));
}
#endif
TEST_CASE("Runtime keyboard groups contain active-screen controls in authored tab order", "[runtime][lazy][focus]") {
    auto* previousGroup=lv_group_get_default();auto* group=lv_group_create();lv_group_set_default(group);
    auto document=Project();document["screens"][0]["obj"][0]["base"]["accessibility"]={{"tabIndex",2}};
    document["screens"][0]["obj"][1]["base"]["accessibility"]={{"tabIndex",0}};
    document["screens"][1]["obj"].push_back(Widget(30,"button",json::object()));
    auto runtime=Load(document,LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE);
    REQUIRE(lv_group_get_obj_count(group)==2);auto* entry=lvgl_runtime_object(runtime.get(),0,9);lv_group_focus_obj(entry);lv_group_focus_next(group);
    CHECK(lv_group_get_focused(group)==lvgl_runtime_object(runtime.get(),0,7));
    REQUIRE(lvgl_runtime_show_screen(runtime.get(),1));Timers();CHECK(lv_group_get_obj_count(group)==1);
    CHECK(lv_group_get_focused(group)==lvgl_runtime_object(runtime.get(),1,30));
    runtime.reset();lv_group_set_default(previousGroup);lv_group_delete(group);
}
#if LVSERIAL_USE_ADVANCED_KEYBOARD
TEST_CASE("Lazy reloading retains a custom trie after owned Protobuf input is released", "[runtime][lazy][prediction]") {
    const uint8_t model[]={'N','G','T','R',3,0,0,0,4,0,0,0,0,0,0,0,0,2,0,2,'a',1,1,'a',1,1};
    std::string id;auto tries=Serialization::ProjectTries::Import(json::array(),"tiny.ngtr",model,sizeof model,&id);
    auto doc=Project();doc["tries"]=tries;
    doc["screens"][2]["obj"].push_back(Widget(3,"advancedKeyboard",{{"trieId",id},{"autoFocus",false},{"targetOid",-1}}));
    auto runtime=Load(doc,LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE);
    for(int i=0;i<3;++i) {
        REQUIRE(lvgl_runtime_show_screen(runtime.get(),2));Timers();auto* keyboard=lvgl_runtime_object(runtime.get(),2,3);REQUIRE(keyboard);
        auto* predictor=lv_advanced_keyboard_get_predictor(keyboard);REQUIRE(predictor);CHECK(lv_advanced_keyboard_predictor_node_count(predictor)==4);
        REQUIRE(lvgl_runtime_show_screen(runtime.get(),0));Timers();CHECK(lvgl_runtime_screen(runtime.get(),2)==nullptr);
    }
}
#endif
