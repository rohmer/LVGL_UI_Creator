#include "catch.hpp"
#include <Serialization/Area.h>

TEST_CASE("Serialization Area")
{
	const json j = Serialization::Area::ToJSON(1, 3, 2, 4);
	REQUIRE(j["x"] == 1);
	REQUIRE(j["width"] == 2);
	REQUIRE(j["y"] == 3);
	REQUIRE(j["height"] == 4);
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
