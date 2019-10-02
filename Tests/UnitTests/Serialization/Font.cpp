#include "catch.hpp"
#include <Serialization/LVFont.h>

TEST_CASE("Serialize Font")
{
	json j = Serialization::LVFont::ToJSON(lv_font_roboto_12);
}

TEST_CASE("Deserialize Font")
{
	json j = Serialization::LVFont::ToJSON(lv_font_roboto_12);
	lv_font_t font = Serialization::LVFont::FromJSON(j);
}