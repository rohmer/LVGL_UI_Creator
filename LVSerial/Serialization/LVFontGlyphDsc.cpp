#include "LVFontGlyphDsc.h"

namespace Serialization
{
	json LVFontGlyphDsc::ToJSON(const lv_font_fmt_txt_glyph_dsc_t &glyphDsc)
	{
		json j;
		j["bitmapIdx"] = (uint32_t)glyphDsc.bitmap_index;
		j["adv_w"] = (uint32_t)glyphDsc.adv_w;
		j["box_h"] = glyphDsc.box_h;
		j["box_w"] = glyphDsc.box_w;
		j["ofs_x"] = glyphDsc.ofs_x;
		j["ofs_y"] = glyphDsc.ofs_y;
		return j;
	}
}