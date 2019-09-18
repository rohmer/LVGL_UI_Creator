#include "Style.h"

namespace Serialization
{	
	json Style::Serialize(lv_style_t &style)
	{
		json j;

		bool glass = false;
		if (style.glass == 1)
			glass = true;
		j["glass"] = glass;

		j["body"]["border"]["color"] = style.body.border.color.full;
		j["body"]["border"]["opa"] = style.body.border.opa;
		j["body"]["border"]["part"] = style.body.border.part;
		j["body"]["border"]["width"] = style.body.border.width;
		j["body"]["gradColor"]= style.body.grad_color.full;		
		j["body"]["main_color"] = style.body.main_color.full;
		j["body"]["opa"]=style.body.opa;
		j["body"]["padding"]["bottom"] = style.body.padding.bottom;
		j["body"]["padding"]["inner"] = style.body.padding.inner;
		j["body"]["padding"]["left"] = style.body.padding.left;
		j["body"]["padding"]["right"] = style.body.padding.right;
		j["body"]["padding"]["top"] = style.body.padding.top;
		j["body"]["radius"]=style.body.radius;
		j["body"]["shadow"]["color"]=style.body.shadow.color.full;
		j["body"]["shadow"]["type"] = style.body.shadow.type;
		j["body"]["shadow"]["width"] = style.body.shadow.width;
		j["image"]["color"] = style.image.color.full;
		j["image"]["intense"] = style.image.intense;
		j["image"]["opa"] = style.image.opa;
		j["line"]["color"] = style.line.color.full;
		j["line"]["opa"] = style.line.opa;
		j["line"]["rounded"] = (uint8_t)style.line.rounded;
		j["line"]["width"] = style.line.width;
		j["text"]["color"]=style.text.color.full;
		//TODO: Deal with fonts correctly
		//j["text"]["font"] = (std::string)typeid(style.text.font).name;
		j["text"]["letter_space"] = style.text.letter_space;
		j["text"]["line_space"] = style.text.line_space;
		j["text"]["opa"] = style.text.opa;
		j["text"]["sel_color"] = style.text.sel_color.full;				

		return j;
	}

	lv_style_t Style::Deserialize(json j)
	{
		lv_style_t style;
		if (j["glass"] == true)
			style.glass = true;
		else
			style.glass = false;		

		style.body.border.color.full= j["body"]["border"]["color"];
		style.body.border.opa=j["body"]["border"]["opa"];
		style.body.border.part=j["body"]["border"]["part"];
		style.body.border.width=j["body"]["border"]["width"];
		style.body.grad_color.full=j["body"]["gradColor"];
		style.body.main_color.full=j["body"]["main_color"];
		style.body.opa=j["body"]["opa"];
		style.body.padding.bottom=j["body"]["padding"]["bottom"];
		style.body.padding.inner=j["body"]["padding"]["inner"];
		style.body.padding.left=j["body"]["padding"]["left"];
		style.body.padding.right=j["body"]["padding"]["right"];
		style.body.padding.top=j["body"]["padding"]["top"];
		style.body.radius=j["body"]["radius"];
		style.body.shadow.color.full=j["body"]["shadow"]["color"];
		style.body.shadow.type=j["body"]["shadow"]["type"];
		style.body.shadow.width=j["body"]["shadow"]["width"];
		style.image.color.full=j["image"]["color"];
		style.image.intense=j["image"]["intense"];
		style.image.opa=j["image"]["opa"];
		style.line.color.full=j["line"]["color"];
		style.line.opa=j["line"]["opa"];
		style.line.rounded=j["line"]["rounded"];
		style.line.width=j["line"]["width"];
		style.text.color.full=j["text"]["color"];
		//TODO: Deal with fonts correctly
		//j["text"]["font"] = typeid(style.text.font).name;
		style.text.letter_space=j["text"]["letter_space"];
		style.text.line_space=j["text"]["line_space"];
		style.text.opa=j["text"]["opa"];
		style.text.sel_color.full=j["text"]["sel_color"];

		return style;
	}
}