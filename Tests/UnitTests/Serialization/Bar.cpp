#include <lvgl/lvgl.h>
#if LVGL_VERSION_MAJOR >= 9
#include "catch.hpp"
#include <Serialization/LVBar.h>
TEST_CASE("Bar properties round trip")
{
    using Serialization::LVBar;
    auto* source = lv_bar_create(lv_screen_active());
    lv_bar_set_range(source, -12, 101);
    lv_bar_set_value(source, 50, LV_ANIM_OFF);
    lv_bar_set_mode(source, LV_BAR_MODE_SYMMETRICAL);
    lv_obj_set_style_anim_duration(source, 81, LV_PART_MAIN);
    lv_obj_set_style_bg_color(source, lv_color_hex(0x125634), LV_PART_INDICATOR);

    const auto saved = LVBar::ToJSON(source);
    auto* restored = LVBar::FromJSON(saved);
    REQUIRE(restored != nullptr);
    CHECK(lv_bar_get_min_value(restored) == -12);
    CHECK(lv_bar_get_max_value(restored) == 101);
    CHECK(lv_bar_get_value(restored) == 50);
    CHECK(lv_bar_get_mode(restored) == LV_BAR_MODE_SYMMETRICAL);
    CHECK(lv_obj_get_style_anim_duration(restored, LV_PART_MAIN) == 81u);
    CHECK(lv_color_to_int(lv_obj_get_style_bg_color(restored, LV_PART_INDICATOR)) == 0x125634);
    lv_obj_delete(restored);
    lv_obj_delete(source);
}
TEST_CASE("Bar native styles remain authoritative while legacy style records still load")
{
    using Serialization::LVBar;
    auto* source = lv_bar_create(lv_screen_active());
    lv_obj_set_pos(source, 12, 14);lv_obj_set_size(source, 80, 24);
    const auto saved = LVBar::ToJSON(source);
    auto* restored = LVBar::FromJSON(saved);
    REQUIRE(restored != nullptr);
    CHECK(LVBar::ToJSON(restored) == saved);
    lv_obj_delete(restored);

    auto legacy = saved;
    legacy["base"].erase("nativeStyles");
    legacy["bar"]["styleInd"]["body"]["main_color"] = 0x125634;
    restored = LVBar::FromJSON(legacy);
    REQUIRE(restored != nullptr);
    CHECK(lv_color_to_int(lv_obj_get_style_bg_color(restored, LV_PART_INDICATOR)) == 0x125634);
    lv_obj_delete(restored);lv_obj_delete(source);
}
#else
#include "catch.hpp"
#include <lvgl/lvgl.h>
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
	lv_obj_del(bar);
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
	lv_obj_del(newBar);
}
#endif
