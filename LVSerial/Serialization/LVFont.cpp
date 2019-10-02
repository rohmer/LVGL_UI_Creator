#include "LVFont.h"
namespace Serialization
{
	json LVFont::ToJSON(const lv_font_t font)
	{
		json j;
		j["baseLine"] = font.base_line;
		j["lineHeight"]=font.line_height;
		lv_font_fmt_txt_dsc_t *dsc = (lv_font_fmt_txt_dsc_t*)font.dsc;
		j["dsc"]=LVFontDsc::ToJSON(*dsc, font);
		return j;
	}

	lv_font_t& LVFont::FromJSON(json j)
	{
		lv_font_t newFont;
		if (j["baseLine"].is_number())
			newFont.base_line = j["baseLine"].get<uint8_t>();
		if (j["lineHeight"].is_number())
			newFont.line_height = j["baseLine"].get<uint8_t>();
		if (j["dsc"].is_object())
			newFont.dsc = LVFontDsc::FromJSON(j["dsc"]);
		return newFont;
	}

}