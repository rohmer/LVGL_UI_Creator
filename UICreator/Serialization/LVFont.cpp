#include "LVFont.h"
namespace Serialization
{
	json LVFont::ToJSON(lv_font_t& font)
	{
		json j;
		j["baseLine"] = font.base_line;
		j["lineHeight"]=font.line_height;
		lv_font_fmt_txt_dsc_t *dsc = (lv_font_fmt_txt_dsc_t*)font.dsc;
		j["dsc"]=LVFontDsc::ToJSON(*dsc, font);
		return j;
	}
}