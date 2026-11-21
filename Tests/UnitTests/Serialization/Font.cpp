#include <lvgl/lvgl.h>
#if LVGL_VERSION_MAJOR >= 9
#include "catch.hpp"
#include <Serialization/LVFont.h>
TEST_CASE("Font metrics serialize and restore")
{
    const auto font = *LV_FONT_DEFAULT;
    const auto record = Serialization::LVFont::ToJSON(font);
    CHECK(record["baseLine"] == font.base_line);
    CHECK(record["lineHeight"] == font.line_height);
    const auto& restored = Serialization::LVFont::FromJSON(record);
    CHECK(restored.line_height == font.line_height);
    CHECK(restored.base_line == font.base_line);
    lv_font_glyph_dsc_t originalGlyph{};
    lv_font_glyph_dsc_t restoredGlyph{};
    REQUIRE(lv_font_get_glyph_dsc(&font, &originalGlyph, 'A', 'V'));
    REQUIRE(lv_font_get_glyph_dsc(&restored, &restoredGlyph, 'A', 'V'));
    CHECK(restoredGlyph.adv_w == originalGlyph.adv_w);
    CHECK(restoredGlyph.box_w == originalGlyph.box_w);
    CHECK(restoredGlyph.box_h == originalGlyph.box_h);
}
#else
#include "catch.hpp"
#include <Serialization/LVFont.h>

TEST_CASE("Serialize Font")
{
	json j = Serialization::LVFont::ToJSON(lv_font_roboto_12);
}

TEST_CASE("Deserialize Font")
{
	json j = Serialization::LVFont::ToJSON(lv_font_roboto_12);
	//TODO: Get Deserialization Done
	//	lv_font_t font = Serialization::LVFont::FromJSON(j);
}
#endif
