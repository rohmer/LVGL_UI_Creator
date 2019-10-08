#include "catch.hpp"
#include "../3rdParty/LVGL/lvgl/lvgl.h"
#include <Serialization/LVBar.h>

TEST_CASE("Serialize Bar")
{
	lv_obj_t* bar = lv_bar_create(lv_scr_act(), nullptr);
	lv_bar_set_value(bar, 50, LV_ANIM_ON);
	lv_bar_set_range(bar, 1, 101);
	lv_bar_set_sym(bar, true);
	static lv_style_t style;
	lv_bar_set_style(bar, LV_BAR_STYLE_BG, &lv_style_plain);
	lv_bar_set_style(bar, LV_BAR_STYLE_INDIC, &lv_style_plain);
	json j = Serialization::LVBar::ToJSON(bar);
	REQUIRE(j["bar"]["range"]["max"] == 101);
	REQUIRE(j["bar"]["range"]["min"] == 1);
	REQUIRE(lv_bar_get_sym(bar) == true);	
}

TEST_CASE("Deserialize Bar")
{
	lv_obj_t* bar = lv_bar_create(lv_scr_act(), nullptr);
	lv_bar_set_value(bar, 50, LV_ANIM_ON);
	lv_bar_set_range(bar, 1, 101);
	lv_bar_set_sym(bar, true);
	static lv_style_t style;
	lv_bar_set_style(bar, LV_BAR_STYLE_BG, &lv_style_plain);
	lv_bar_set_style(bar, LV_BAR_STYLE_INDIC, &lv_style_plain);
	json j = Serialization::LVBar::ToJSON(bar);

	lv_obj_t* newBar = Serialization::LVBar::FromJSON(j);
	REQUIRE(lv_bar_get_min_value(bar) == lv_bar_get_min_value(newBar));
	REQUIRE(lv_bar_get_max_value(bar) == lv_bar_get_max_value(newBar));
	REQUIRE(lv_bar_get_sym(bar) == lv_bar_get_sym(newBar));
}
