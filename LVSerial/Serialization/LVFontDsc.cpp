#include "LVFontDsc.h"

namespace Serialization
{
	void *LVFontDsc::FromJSON(json j)
	{
		lv_font_fmt_txt_dsc_t fontDsc;
		if (j["bitmapFmt"].is_number())
			fontDsc.bitmap_format = j["bitmapFmt"].get<uint16_t>();
		if (j["bpp"].is_number())
			fontDsc.bpp = j["bpp"].get<uint16_t>();
		if (j["cmapNum"].is_number())
			fontDsc.cmap_num = j["cmapNum"].get<uint16_t>();
		if (j["kernScale"].is_number())
			fontDsc.kern_scale = j["kernScale"];
		if (j["kernClases"].is_number())
			fontDsc.kern_classes = j["kernClasses"];
		if (j["lastGlyphID"].is_number())
			fontDsc.last_glyph_id = j["lastGlyphID"].get<uint32_t>();
		if (j["lastLetter"].is_number())
			fontDsc.last_glyph_id = j["lastLetter"].get<uint32_t>();
		if(j["cmaps"].is_array())
		{
			std::vector<lv_font_fmt_txt_cmap_t> cmapVec;
			int i = 0;
			while(j["cmaps"][i].is_object())
			{
				cmapVec.push_back(LVFontCMap::FromJSON(j["cmaps"][i]));
				i++;
			}

			fontDsc.cmap_num = cmapVec.size();
			fontDsc.cmaps = &cmapVec[0];
		}
		return (void*)&fontDsc;
		
	}

	json LVFontDsc::ToJSON(lv_font_fmt_txt_dsc_t &fontDsc, const lv_font_t font)
	{
		json j;
		j["bitmapFmt"] = (int)fontDsc.bitmap_format;
		j["bpp"] = (int)fontDsc.bpp;
		j["cmapNum"] = (int)fontDsc.cmap_num;
		j["kernScale"] = (int)fontDsc.kern_scale;
		j["kernClasses"] = (int)fontDsc.kern_classes;
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