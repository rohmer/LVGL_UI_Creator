#include <lvgl/lvgl.h>
#if LVGL_VERSION_MAJOR >= 9
#include "catch.hpp"
#include <Serialization/LVObject.h>
TEST_CASE("Legacy realign metadata survives a round trip")
{
    auto legacy = json::parse(R"({"coords":{"x":10,"y":20,"width":80,"height":50},"realign":{"xofs":12,"yofs":-2,"autore":true,"origoalign":false,"align":1}})");
    auto* object = Serialization::LVObject::FromJSON(legacy);
    REQUIRE(object != nullptr);
    const auto saved = Serialization::LVObject::ToJSON(object);
    CHECK(saved["realign"] == legacy["realign"]);
    CHECK(saved["coords"] == legacy["coords"]);
    lv_obj_delete(object);
}
#else
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
#endif
