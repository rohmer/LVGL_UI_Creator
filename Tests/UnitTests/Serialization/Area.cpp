#include "catch.hpp"
#include <Serialization/Area.h>

TEST_CASE("Serialization Area")
{
	lv_area_t coord;
	coord.x1 = 1;
	coord.x2 = 2;
	coord.y1 = 3;
	coord.y2 = 4;
	json j = Serialization::Area::ToJSON(coord.x1, coord.y1, coord.x2, coord.y1);
	REQUIRE(j["x1"] == 1);
	REQUIRE(j["x2"] == 2);
	REQUIRE(j["y1"] == 3);
	REQUIRE(j["y2"] == 4);
}

TEST_CASE("Deserialization","[Area]")
{
	json j;
	j["x"] = 32;
	j["width"] = 11;
	j["y"] = 531;
	j["height"] = 523;
    Serialization::sAreaPos coord = Serialization::Area::FromJSON(j);
	REQUIRE(coord.x == 32);
	REQUIRE(coord.width == 11);
	REQUIRE(coord.y == 531);
	REQUIRE(coord.height == 523);

}