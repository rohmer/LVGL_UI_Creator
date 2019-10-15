#include "LVFontCMap.h"

namespace Serialization
{
    lv_font_fmt_txt_cmap_t LVFontCMap::FromJSON(json j)
    {
        lv_font_fmt_txt_cmap_t cmap;
        if (j["rangeStart"].is_number())
            cmap.range_start = j["rangeStart"];
        if (j["rangeLength"].is_number())
            cmap.range_length = j["rangeLength"];
        if (j["type"].is_number())
            cmap.type = j["type"];
        if (j["glyphIDstart"].is_number())
            cmap.glyph_id_start = j["glyphIDstart"];
        if (cmap.type <= 1)
        {
            if (j["glyphIDOffsetList"].is_array())
            {
                std::vector<uint8_t> glyphIdOff;
                for (json::iterator it = j["glyphIDOffsetList"].begin();
                     it != j["glyphIDOffsetList"].end();
                     ++it)
                {
                    glyphIdOff.push_back(*it);
                }
                cmap.glyph_id_ofs_list = &glyphIdOff[0];
            }
            else
            {
                cmap.glyph_id_ofs_list = nullptr;
            }
        }
        else
        {
            if (j["glyphIDOffsetList"].is_array())
            {
                std::vector<uint16_t> glyphIdOff;
                for (json::iterator it = j["glyphIDOffsetList"].begin();
                     it != j["glyphIDOffsetList"].end();
                     ++it)
                {
                    glyphIdOff.push_back(*it);
                }
                cmap.glyph_id_ofs_list = &glyphIdOff[0];
            }
            else
            {
                cmap.glyph_id_ofs_list = nullptr;
            }
        }
        if (j["unicodeList"].is_array())
        {
            std::vector<uint16_t> ul;
            for (json::iterator it = j["unicodeList"].begin();
                 it != j["unicodeList"].end();
                 ++it)
            {
                ul.push_back(*it);
            }
            cmap.unicode_list = new uint16_t[ul.size()];
            cmap.unicode_list = &ul[0];
        }

        return cmap;
    }

    json LVFontCMap::ToJSON(lv_font_fmt_txt_cmap_t& cmap)
    {
        json j;
        j["rangeStart"] = cmap.range_start;
        j["rangeLength"] = cmap.range_length;
        j["type"] = static_cast<uint8_t>(cmap.type);
        j["glyphIDstart"] = cmap.glyph_id_start;
        if (cmap.type <= 1)
        {
            const uint8_t* ptr = (uint8_t*)cmap.glyph_id_ofs_list;
            if (ptr == nullptr)
            {
                j["glyphIDOffsetList"] = nullptr;
            }
            else
            {
                for (int i = 0; i < cmap.list_length; i++)
                {
                    j["glyphIDOffsetList"][i] = static_cast<uint8_t>(*ptr++);
                }
            }
        }
        else
        {
            const uint16_t* ptr = (uint16_t*)cmap.glyph_id_ofs_list;
            if (ptr == nullptr)
            {
                j["glyphIDOffsetList"] = NULL;
            }
            else
            {
                for (int i = 0; i < cmap.list_length; i++)
                {
                    j["glyphIDOffsetList"][i] = static_cast<uint16_t>(*ptr++);
                }
            }
        }

        uint16_t* ptr = (uint16_t*)cmap.unicode_list;
        for (int i = 0; i < cmap.list_length; i++)
        {
            j["unicodeList"][i] = static_cast<uint16_t>(*ptr++);
        }

        return j;
    }
}
