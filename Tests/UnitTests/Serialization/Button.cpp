#include <lvgl/lvgl.h>
#if LVGL_VERSION_MAJOR >= 9
#include "catch.hpp"
#include <Serialization/LVButton.h>
TEST_CASE("Legacy button layout, fit and style load")
{
    using Serialization::LVButton;
    auto input = json::parse(R"({
      "base": {"name":"Button", "coords":{"x":10,"y":20,"width":80,"height":50}},
      "button": {"layout":2,"toggle":true,"fit":{"left":1,"right":1,"top":1,"bottom":1},
                 "ink":{"in":999,"out":998,"wait":997},
                 "styles":{"rel":{"body":{"main_color":4278190335}}}}
    })");
    auto* restored = LVButton::FromJSON(input);
    REQUIRE(restored != nullptr);
    const auto saved = LVButton::ToJSON(restored);
    CHECK(lv_obj_is_checkable(restored));
    CHECK(lv_obj_get_style_flex_flow(restored, LV_PART_MAIN) == LV_FLEX_FLOW_COLUMN);
    CHECK(lv_color_to_int(lv_obj_get_style_bg_color(restored, LV_PART_MAIN)) == 255);
    CHECK(saved["button"]["fit"] == input["button"]["fit"]);
    CHECK(saved["button"]["ink"] == input["button"]["ink"]);
    lv_obj_delete(restored);
}
#else
#include "catch.hpp"
#include <lvgl/lvgl.h>
#include <Serialization/LVButton.h>

TEST_CASE("Serialize Button")
{
	lv_obj_t* button = lv_btn_create(lv_scr_act(), nullptr);
	lv_btn_set_layout(button, LV_LAYOUT_COL_L);
	lv_btn_set_fit4(button, LV_FIT_TIGHT, LV_FIT_TIGHT, LV_FIT_TIGHT, LV_FIT_TIGHT);
	lv_btn_set_ink_in_time(button, 999);
	lv_btn_set_ink_out_time(button, 998);
	lv_btn_set_ink_wait_time(button, 997);
	json j = Serialization::LVButton::ToJSON(button);
	REQUIRE(j["button"]["layout"] == LV_LAYOUT_COL_L);
	REQUIRE(j["button"]["fit"]["bottom"] == LV_FIT_TIGHT);
	REQUIRE(j["button"]["fit"]["top"] == LV_FIT_TIGHT);
	REQUIRE(j["button"]["fit"]["right"] == LV_FIT_TIGHT);
	REQUIRE(j["button"]["fit"]["left"] == LV_FIT_TIGHT);
	REQUIRE(j["button"]["ink"]["in"] == 999);
	REQUIRE(j["button"]["ink"]["out"]== 998);
	REQUIRE(j["button"]["ink"]["wait"] == 997);
	lv_obj_del(button);
}

TEST_CASE("Deserialize Button")
{
	lv_obj_t* button = lv_btn_create(lv_scr_act(), nullptr);
	lv_btn_set_layout(button, LV_LAYOUT_COL_L);
	lv_btn_set_fit4(button, LV_FIT_TIGHT, LV_FIT_TIGHT, LV_FIT_TIGHT, LV_FIT_TIGHT);
	lv_btn_set_ink_in_time(button, 999);
	lv_btn_set_ink_out_time(button, 998);
	lv_btn_set_ink_wait_time(button, 997);
	json j = Serialization::LVButton::ToJSON(button);
	lv_obj_t* nButton = Serialization::LVButton::FromJSON(j);
	REQUIRE(lv_btn_get_layout(button) == lv_btn_get_layout(nButton));
	REQUIRE(lv_btn_get_fit_bottom(button) == lv_btn_get_fit_bottom(nButton));
	REQUIRE(lv_btn_get_fit_top(button) == lv_btn_get_fit_top(nButton));
	REQUIRE(lv_btn_get_fit_left(button) == lv_btn_get_fit_left(nButton));
	REQUIRE(lv_btn_get_fit_right(button) == lv_btn_get_fit_right(nButton));
	REQUIRE(lv_btn_get_ink_in_time(button) == lv_btn_get_ink_in_time(nButton));
	REQUIRE(lv_btn_get_ink_out_time(button) == lv_btn_get_ink_out_time(nButton));
	REQUIRE(lv_btn_get_ink_wait_time(button) == lv_btn_get_ink_wait_time(nButton));
	lv_obj_del(button);
	lv_obj_del(nButton);
	
}
#endif
