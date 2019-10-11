#include "catch.hpp"
#include "../3rdParty/LVGL/lvgl/lvgl.h"
#include <Serialization/LVArc.h>

TEST_CASE("Serialize Arc")
{
	lv_obj_t* arc = lv_arc_create(lv_scr_act(), nullptr);
	lv_arc_set_angles(arc, 25, 125);
	json j = Serialization::LVArc::ToJSON(arc);
	REQUIRE(j["arc"]["angle"]["start"] == 25);
	REQUIRE(j["arc"]["angle"]["end"] == 125);
	lv_obj_del(arc);
}

TEST_CASE("Deserialize Arc")
{
	lv_obj_t* arc = lv_arc_create(lv_scr_act(), nullptr);
	lv_arc_set_angles(arc, 25, 125);
	static lv_style_t style;
	lv_style_copy(&style, &lv_style_plain);
	style.line.color = lv_color_make(0,0,255);           /*Arc color*/
	style.line.width = 8;                       /*Arc width*/
	style.line.rounded = 1;
	lv_arc_set_style(arc, LV_ARC_STYLE_MAIN, &style);	
	json j = Serialization::LVArc::ToJSON(arc);

	lv_obj_t* newArc = Serialization::LVArc::FromJSON(j);
	static const lv_style_t *newStyle=lv_arc_get_style(arc, LV_ARC_STYLE_MAIN);
	int start, end;
	start = lv_arc_get_angle_start(arc);
	end = lv_arc_get_angle_end(arc);
	REQUIRE(start == 25);
	REQUIRE(end == 125);
	REQUIRE(newStyle->line.color.ch.blue == 255);
	REQUIRE(newStyle->line.color.ch.green == 0);
	REQUIRE(newStyle->line.color.ch.red == 0);
	REQUIRE(newStyle->line.width == 8);
	REQUIRE(newStyle->line.rounded == 1);
	lv_obj_del(arc);
	lv_obj_del(newArc);
}