#include "catch.hpp"
#include <Serialization/Realign.h>

TEST_CASE("Serialization Realign")
{
	lv_reailgn_t realign;
	realign.align = LV_ALIGN_IN_BOTTOM_LEFT;
	realign.auto_realign = 1;
	realign.origo_align = 0;
	realign.xofs = -12;
	realign.yofs = 22;
	json j = Serialization::Realign::ToJSON(realign);
	REQUIRE(j["align"] == LV_ALIGN_IN_BOTTOM_LEFT);
	REQUIRE(j["autore"] == true);
	REQUIRE(j["origoalign"] == false);
	REQUIRE(j["xofs"] == -12);
	REQUIRE(j["yofs"] == 22);
}