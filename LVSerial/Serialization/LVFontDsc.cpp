#include "LVFontDsc.h"

namespace Serialization
{
    void* LVFontDsc::FromJSON(json j)
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
        if (j["cmaps"].is_array())
        {
            std::vector<lv_font_fmt_txt_cmap_t> cmapVec;
            int i = 0;
            while (j["cmaps"][i].is_object())
            {
                cmapVec.push_back(LVFontCMap::FromJSON(j["cmaps"][i]));
                i++;
            }

            fontDsc.cmap_num = cmapVec.size();
            fontDsc.cmaps = new lv_font_fmt_txt_cmap_t[cmapVec.size()];
            fontDsc.cmaps = &cmapVec[0];
        }
        int glyphDsc = 0;
        std::vector<lv_font_fmt_txt_glyph_dsc_t> gd;
        while (j["GlyphDsc"][glyphDsc].is_object())
        {
            gd.push_back(*LVFontGlyphDsc::FromJSON(j["GlyphDsc"][glyphDsc]));
            glyphDsc++;
        }
        fontDsc.glyph_dsc = new lv_font_fmt_txt_glyph_dsc_t[gd.size()];
        fontDsc.glyph_dsc = &gd[0];
        if (j["Kern"].is_object())
        {
            fontDsc = LVFontKernDSC::FromJSON(fontDsc, fontDsc.kern_classes, j["Kern"]);
        }
        return static_cast<void*>(&fontDsc);
    }

    json LVFontDsc::ToJSON(lv_font_fmt_txt_dsc_t& fontDsc, const lv_font_t font)
    {
        json j;
        j["bitmapFmt"] = static_cast<int>(fontDsc.bitmap_format);
        j["bpp"] = static_cast<int>(fontDsc.bpp);
        j["cmapNum"] = static_cast<int>(fontDsc.cmap_num);
        j["kernScale"] = static_cast<int>(fontDsc.kern_scale);
        j["kernClasses"] = static_cast<int>(fontDsc.kern_classes);
        j["lastGlyphID"] = static_cast<uint32_t>(fontDsc.last_glyph_id);
        j["lastLetter"] = static_cast<uint32_t>(fontDsc.last_letter);
        lv_font_fmt_txt_cmap_t* ptr = (lv_font_fmt_txt_cmap_t *)fontDsc.cmaps;
        int glyphRange = 0;
        for (int i = 0; i < fontDsc.cmap_num; i++)
        {
            glyphRange += ptr->list_length;
            j["cmaps"][i] = LVFontCMap::ToJSON(*ptr++);
        }
        for (int i = 0; i < glyphRange; i++)
        {
            j["GlyphDsc"][i] =
                LVFontGlyphDsc::ToJSON(fontDsc.glyph_dsc[i]);
        }
        j["Kern"] = LVFontKernDSC::ToJSON(fontDsc, fontDsc.kern_classes);
        return j;
    }
}
