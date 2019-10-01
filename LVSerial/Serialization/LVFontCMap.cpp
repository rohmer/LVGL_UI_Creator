#include "LVFontCMap.h"

namespace Serialization
{
	json LVFontCMap::ToJSON(lv_font_fmt_txt_cmap_t  &cmap)
	{
		json j;
		j["rangeStart"] = cmap.range_start;
		j["rangeLength"] = cmap.range_length;
		j["type"] = (uint8_t)cmap.type;
		j["glyphIDstart"] = cmap.glyph_id_start;
		if (cmap.type <= 1)
		{
			const uint8_t *ptr = (uint8_t*)cmap.glyph_id_ofs_list;
			if (ptr == NULL)
			{
				j["glyphIDOffsetList"] = NULL;
			}
			else
			{
				for (int i = 0; i < cmap.list_length; i++)
				{
					j["glyphIDOffsetList"][i] = (uint8_t)*ptr++;
				}
			}
		}
		else
		{
			const uint16_t *ptr = (uint16_t*)cmap.glyph_id_ofs_list;
			if (ptr == NULL)
			{
				j["glyphIDOffsetList"] = NULL;
			}
			else
			{
				for (int i = 0; i < cmap.list_length; i++)
				{
					j["glyphIDOffsetList"][i] = (uint16_t)*ptr++;
				}
			}
		}

		uint16_t* ptr = (uint16_t*)cmap.unicode_list;
		for(int i=0; i<cmap.list_length; i++)
		{
			j["unicodeList"][i] = (uint16_t)*ptr++;
		}

		return j;
	}
}