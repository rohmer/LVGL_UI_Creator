#include "LVFontKernDSC.h"

namespace Serialization
{
	json LVFontKernDSC::ToJSON(lv_font_fmt_txt_dsc_t &fontDsc, int kernClasses)
	{
		json j;
		const lv_font_fmt_txt_kern_pair_t * kdsc =
			(const lv_font_fmt_txt_kern_pair_t*)fontDsc.kern_dsc;
		if (kernClasses == 0)
		{
			if (kdsc->glyph_ids_size == 0)
			{
				uint8_t* gids = (uint8_t*)kdsc->glyph_ids;
				for (int i = 0; i < kdsc->pair_cnt; i++)
				{
					j["GlyphIDs"][i] = *gids;
					gids++;
				}
			}
		}
		if (kernClasses == 1)
		{
			if (kdsc->glyph_ids_size == 0)
			{
				uint16_t* gids = (uint16_t*)kdsc->glyph_ids;
				for (int i = 0; i < kdsc->pair_cnt; i++)
				{
					j["GlyphIDs"][i] = *gids;
					gids++;
				}
			}
		}
		int8_t* values = (int8_t*)kdsc->values;
		for(int i=0; i<kdsc->pair_cnt; i++)
		{
			j["Values"][i] = *values;
			values++;
		}
		return j;
	}
}