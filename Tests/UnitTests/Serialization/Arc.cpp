#include <lvgl/lvgl.h>
#if LVGL_VERSION_MAJOR >= 9
#include "catch.hpp"
#include <Serialization/LVArc.h>
TEST_CASE("Arc properties round trip")
{
    using Serialization::LVArc;
    auto* source = LVArc::Create(lv_screen_active());
    REQUIRE(LVArc::SetValue(source, "/arc/angle/start", 25));
    REQUIRE(LVArc::SetValue(source, "/arc/angle/end", 125));
    REQUIRE(LVArc::SetValue(source, "/arc/style/line.width", 8));
    REQUIRE(LVArc::SetValue(source, "/arc/style/line.color", 255));
    REQUIRE(LVArc::SetValue(source, "/arc/style/line.rounded", 1));

    const auto saved = LVArc::ToJSON(source);
    auto* restored = LVArc::FromJSON(saved);
    REQUIRE(restored != nullptr);
    CHECK(lv_arc_get_bg_angle_start(restored) == 25);
    CHECK(lv_arc_get_bg_angle_end(restored) == 125);
    CHECK(lv_obj_get_style_arc_width(restored, LV_PART_MAIN) == 8);
    CHECK(lv_color_to_int(lv_obj_get_style_arc_color(restored, LV_PART_MAIN)) == 255);
    CHECK(lv_obj_get_style_arc_rounded(restored, LV_PART_MAIN));
    lv_obj_delete(restored);
    lv_obj_delete(source);
}
#else
#include "catch.hpp"
#include <lvgl/lvgl.h>
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
#endif
