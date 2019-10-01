#include "Style.h"

namespace Serialization
{
	json Style::ToJSON(lv_style_t &style)
	{
		std::string name = "";
		if (StyleComp(style, lv_style_btn_pr))
		{
			name = "lv_style_btn_pr";
		}
		if (StyleComp(style, lv_style_scr))
		{
			name = "lv_style_scr";
		}
		if (StyleComp(style, lv_style_transp))
		{
			name = "lv_style_transp";
		}
		if (StyleComp(style, lv_style_transp_fit))
		{
			name = "lv_style_transp_fit";
		}
		if (StyleComp(style, lv_style_transp_tight))
		{
			name = "lv_style_transp_tight";
		}
		if (StyleComp(style, lv_style_plain))
		{
			name = "lv_style_plain";
		}
		if (StyleComp(style, lv_style_plain_color))
		{
			name = "lv_style_plain_color";
		}
		if (StyleComp(style, lv_style_pretty_color))
		{
			name = "lv_style_pretty_color";
		}
		if (StyleComp(style, lv_style_pretty))
		{
			name = "lv_style_pretty";
		}
		if (StyleComp(style, lv_style_btn_ina))
		{
			name = "lv_style_btn_ina";
		}
		if (StyleComp(style, lv_style_btn_pr))
		{
			name = "lv_style_btn_pr";
		}
		if (StyleComp(style, lv_style_btn_rel))
		{
			name = "lv_style_btn_rel";
		}
		if (StyleComp(style, lv_style_btn_tgl_pr))
		{
			name = "lv_style_btn_tgl_pr";
		}
		if (StyleComp(style, lv_style_btn_tgl_rel))
		{
			name = "lv_style_btn_tgl_rel";
		}
		// if name is still blank, we dont know what it is.  Set a random name
		if (name == "")
		{
			std::stringstream ss;
			srand(time(nullptr));
			ss << "Style_" << (rand() % 65536);
			name = ss.str();
		}
		return ToJSON(style, name);		
	}
	
	json Style::ToJSON(lv_style_t &style, std::string name)
	{
		json j;

		j["name"] = name;
		j["glass"] = (uint8_t)style.glass;
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
		const lv_font_t font = *style.text.font;
		j["text"]["font"] = LVFont::ToJSON(font);
		j["text"]["letter_space"] = style.text.letter_space;
		j["text"]["line_space"] = style.text.line_space;
		j["text"]["opa"] = style.text.opa;
		j["text"]["sel_color"] = style.text.sel_color.full;				

		return j;
	}

	lv_style_t Style::FromJSON(json j)
	{
		lv_style_t style;
		style.glass = j["glass"];
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
		//style.text.font=const_cast<lv_font_t*>(&LVFont::FromJSON(j["text"]["font"]));
		style.text.letter_space=j["text"]["letter_space"];
		style.text.line_space=j["text"]["line_space"];
		style.text.opa=j["text"]["opa"];
		style.text.sel_color.full=j["text"]["sel_color"];		

		return style;
	}

	bool Style::StyleComp(lv_style_t st1, lv_style_t st2)
	{
		if ((st1.body.border.color.full == st2.body.border.color.full)
			&& (st1.body.border.opa == st2.body.border.opa)
			&& (st1.body.border.part == st2.body.border.part)
			&& (st1.body.border.width == st2.body.border.width)
			&& (st1.body.grad_color.full == st2.body.grad_color.full)
			&& (st1.body.main_color.full == st2.body.main_color.full)
			&& (st1.body.opa == st2.body.opa)
			&& (st1.body.padding.bottom == st2.body.padding.bottom)
			&& (st1.body.padding.left == st2.body.padding.left)
			&& (st1.body.padding.right == st2.body.padding.right)
			&& (st1.body.padding.top == st2.body.padding.top)
			&& (st1.body.radius == st2.body.radius)
			&& (st1.body.shadow.color.full && st2.body.shadow.color.full)
			&& (st1.body.shadow.type == st2.body.shadow.type)
			&& (st1.body.shadow.width == st2.body.shadow.width)
			&& (st1.image.color.full == st2.image.color.full)
			&& (st1.image.intense == st2.image.intense)
			&& (st1.image.opa == st2.image.opa)
			&& (st1.line.color.full == st2.line.color.full)
			&& (st1.line.opa == st2.line.opa)
			&& (st1.line.rounded == st2.line.rounded)
			&& (st1.line.width == st2.line.width)
			&& (st1.text.color.full == st2.text.color.full)
			&& (st1.text.letter_space == st2.text.letter_space)
			&& (st1.text.line_space == st2.text.line_space)
			&& (st1.text.opa == st2.text.opa)
			&& (st1.text.sel_color.full == st2.text.sel_color.full))
			return true;

		return false;
	}

}