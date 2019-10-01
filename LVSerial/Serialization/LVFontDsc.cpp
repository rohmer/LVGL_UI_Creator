#include "LVFontDsc.h"

namespace Serialization
{
	json LVFontDsc::ToJSON(lv_font_fmt_txt_dsc_t &fontDsc, const lv_font_t font)
	{
		json j;
		j["bitmapFmt"] = (int)fontDsc.bitmap_format;
		j["bpp"] = (int)fontDsc.bpp;
		j["cmapNum"] = (int)fontDsc.cmap_num;
		j["kernScale"] = (int)fontDsc.kern_scale;
		j["keyClasses"] = (int)fontDsc.kern_classes;
		j["lastGlyphID"]=(uint32_t)fontDsc.last_glyph_id;
		j["lastLetter"]=(uint32_t)fontDsc.last_letter;
		lv_font_fmt_txt_cmap_t * ptr=(lv_font_fmt_txt_cmap_t *)fontDsc.cmaps;
		int glyphRange = 0;
		for(int i=0; i<fontDsc.cmap_num; i++)
		{
			glyphRange += ptr->list_length;
			j["cmaps"][i] = LVFontCMap::ToJSON(*ptr++);
		}
		for(int i=0; i<glyphRange; i++)
		{
			j["GlyphDsc"][i] = 
				LVFontGlyphDsc::ToJSON(fontDsc.glyph_dsc[i]);
		}
		j["Kern"] = LVFontKernDSC::ToJSON(fontDsc, fontDsc.kern_classes);
		return j;
		
	}
}