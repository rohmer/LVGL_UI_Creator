#include "LVFontKernDSC.h"

namespace Serialization
{
    lv_font_fmt_txt_dsc_t LVFontKernDSC::FromJSON(lv_font_fmt_txt_dsc_t& fontDsc,
                                                  int kernClasses,
                                                  json j)
    {
        if (kernClasses == 0)
        {
            if (j["glyphIDSize"] == 0)
            {
                std::vector<uint8_t> glyphIDs;
                int i = 0;
                while (j["GlyphIDs"][i].is_number())
                {
                    glyphIDs.push_back(uint8_t(j["GlyphIDs"][i].get<uint8_t>()));
                    i++;
                }

                static std::vector<uint8_t> kpValues;
                i = 0;
                while (j["Values"][i].is_number())
                {
                    kpValues.push_back(j["Values"][i].get<uint8_t>());
                    i++;
                }
                //const uint8_t* kpArr = new uint8_t[i];
                static uint8_t* kpArr = new uint8_t[i];
                static lv_font_fmt_txt_kern_pair_t* kern_pairs = new lv_font_fmt_txt_kern_pair_t();
                kern_pairs->glyph_ids_size = 0;
                kern_pairs->pair_cnt = glyphIDs.size();
                kern_pairs->glyph_ids = glyphIDs.data();
                kern_pairs->glyph_ids_size = 0;


                fontDsc.kern_dsc = kern_pairs;
            }
            else
            {
                std::vector<uint16_t> glyphIDs;
                int i = 0;
                while (j["GlyphIDs"][i].is_number())
                {
                    glyphIDs.push_back(uint16_t(j["GlyphIDs"][i].get<uint8_t>()));
                    i++;
                }
                static lv_font_fmt_txt_kern_pair_t* kern_pairs = new lv_font_fmt_txt_kern_pair_t();
                kern_pairs->glyph_ids = new uint16_t[glyphIDs.size()];
                kern_pairs->glyph_ids = glyphIDs.data();
                kern_pairs->pair_cnt = glyphIDs.size();
                kern_pairs->glyph_ids_size = j["glyphIDSize"];
                fontDsc.kern_dsc = kern_pairs;
            }
        }

        return fontDsc;
    }

    json LVFontKernDSC::ToJSON(lv_font_fmt_txt_dsc_t& fontDsc, int kernClasses)
    {
        json j;
        const lv_font_fmt_txt_kern_pair_t* kdsc =
            static_cast<const lv_font_fmt_txt_kern_pair_t*>(fontDsc.kern_dsc);
        j["glyphIDSize"] = kdsc->glyph_ids_size;
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
        for (int i = 0; i < kdsc->pair_cnt; i++)
        {
            j["Values"][i] = *values;
            values++;
        }
        return j;
    }
}
