#include "catch.hpp"
#include "../3rdParty/LVGL/lvgl/lvgl.h"
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
	
	
}