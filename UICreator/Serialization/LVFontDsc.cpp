#include "LVFontDsc.h"

namespace Serialization
{
	json LVFontDsc::ToJSON(lv_font_fmt_txt_dsc_t &fontDsc, lv_font_t &font)
	{
		json j;
		j["FontDsc"]["bitmapFmt"] = (int)fontDsc.bitmap_format;
		j["FontDsc"]["bpp"] = (int)fontDsc.bpp;
		j["FontDsc"]["cmapNum"] = (int)fontDsc.cmap_num;
		j["FontDsc"]["kernScale"] = (int)fontDsc.kern_scale;
		j["FontDsc"]["keyClasses"] = (int)fontDsc.kern_classes;
		j["FontDsc"]["lastGlyphID"]=(uint32_t)fontDsc.last_glyph_id;
		j["FontDsc"]["lastLetter"]=(uint32_t)fontDsc.last_letter;
		lv_font_fmt_txt_cmap_t * ptr=(lv_font_fmt_txt_cmap_t *)fontDsc.cmaps;
		int glyphRange = 0;
		for(int i=0; i<fontDsc.cmap_num; i++)
		{
			glyphRange += ptr->list_length;
			j["FontDsc"]["cmaps"][i] = LVFontCMap::ToJSON(*ptr++);
		}
		for(int i=0; i<glyphRange; i++)
		{
			j["FontDsc"]["GlyphDsc"][i] = 
				LVFontGlyphDsc::ToJSON(fontDsc.glyph_dsc[i]);
		}
		j["FontDsc"]["Kern"] = LVFontKernDSC::ToJSON(fontDsc, fontDsc.kern_classes);
		return j;
		
	}
}