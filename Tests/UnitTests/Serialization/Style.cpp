#include "catch.hpp"
#include <Serialization/Style.h>

TEST_CASE("Serialization Style")
{
	lv_style_init();
	static lv_style_t style;
	lv_style_copy(&style, &lv_style_pretty_color);
	json j = Serialization::Style::ToJSON(style);
	REQUIRE(j["name"] == "lv_style_pretty_color"); 
	REQUIRE(j["glass"] == lv_style_pretty_color.glass);
	REQUIRE(j["body"]["border"]["color"] == lv_style_pretty_color.body.border.color.full);
	REQUIRE(j["body"]["border"]["opa"] == lv_style_pretty_color.body.border.opa);
	REQUIRE(j["body"]["border"]["part"] == lv_style_pretty_color.body.border.part);
	REQUIRE(j["body"]["border"]["width"] == lv_style_pretty_color.body.border.width);
	REQUIRE(j["body"]["gradColor"] == lv_style_pretty_color.body.grad_color.full);
	REQUIRE(j["body"]["main_color"] == lv_style_pretty_color.body.main_color.full);
	REQUIRE(j["body"]["opa"] == lv_style_pretty_color.body.opa);
	REQUIRE(j["body"]["padding"]["bottom"] == lv_style_pretty_color.body.padding.bottom);
	REQUIRE(j["body"]["padding"]["inner"] == lv_style_pretty_color.body.padding.inner);
	REQUIRE(j["body"]["padding"]["left"] == lv_style_pretty_color.body.padding.left);
	REQUIRE(j["body"]["padding"]["right"] == lv_style_pretty_color.body.padding.right);
	REQUIRE(j["body"]["padding"]["top"] == lv_style_pretty_color.body.padding.top);
	REQUIRE(j["body"]["radius"] == lv_style_pretty_color.body.radius);
	REQUIRE(j["body"]["shadow"]["color"] == lv_style_pretty_color.body.shadow.color.full);
	REQUIRE(j["body"]["shadow"]["type"] == lv_style_pretty_color.body.shadow.type);
	REQUIRE(j["body"]["shadow"]["width"] == lv_style_pretty_color.body.shadow.width);
	REQUIRE(j["image"]["color"] == lv_style_pretty_color.image.color.full);
	REQUIRE(j["image"]["intense"] == lv_style_pretty_color.image.intense);
	REQUIRE(j["image"]["opa"] == lv_style_pretty_color.image.opa);
	REQUIRE(j["line"]["color"] == lv_style_pretty_color.line.color.full);
	REQUIRE(j["line"]["opa"] == lv_style_pretty_color.line.opa);
	REQUIRE(j["line"]["rounded"] == (uint8_t)lv_style_pretty_color.line.rounded);
	REQUIRE(j["line"]["width"] == lv_style_pretty_color.line.width);
	REQUIRE(j["text"]["color"] == lv_style_pretty_color.text.color.full);
	REQUIRE(j["text"]["letter_space"] == style.text.letter_space);
	REQUIRE(j["text"]["line_space"] == style.text.line_space);
	REQUIRE(j["text"]["opa"] == style.text.opa);
	REQUIRE(j["text"]["sel_color"] == style.text.sel_color.full);
}

TEST_CASE("Deserialization Style")
{
	lv_style_init();
	lv_style_t style;
	lv_style_copy(&style, &lv_style_pretty_color);
	json j = Serialization::Style::ToJSON(style);

	lv_style_t deserStyle = Serialization::Style::FromJSON(j);
	bool same=Serialization::Style::StyleComp(style, deserStyle);
}
