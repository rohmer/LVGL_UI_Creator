#include "LVFontGlyphDsc.h"

namespace Serialization
{
    json LVFontGlyphDsc::ToJSON(const lv_font_fmt_txt_glyph_dsc_t& glyphDsc)
    {
        json j;
        j["bitmapIdx"] = static_cast<uint32_t>(glyphDsc.bitmap_index);
        j["adv_w"] = static_cast<uint32_t>(glyphDsc.adv_w);
        j["box_h"] = glyphDsc.box_h;
        j["box_w"] = glyphDsc.box_w;
        j["ofs_x"] = glyphDsc.ofs_x;
        j["ofs_y"] = glyphDsc.ofs_y;
        return j;
    }

    lv_font_fmt_txt_glyph_dsc_t* LVFontGlyphDsc::FromJSON(json j)
    {
        lv_font_fmt_txt_glyph_dsc_t* gd = new lv_font_fmt_txt_glyph_dsc_t();

        if (j["bitmapIdx"].is_number())
            gd->bitmap_index = j["bitmapIdx"].get<uint32_t>();
        if (j["adv_w"].is_number())
            gd->adv_w = j["adv_w"].get<uint32_t>();
        if (j["box_h"].is_number())
            gd->box_h = j["box_h"];
        if (j["box_w"].is_number())
            gd->box_w = j["box_w"];
        if (j["ofs_x"].is_number())
            gd->ofs_x = j["ofs_x"];
        if (j["ofs_y"].is_number())
            gd->ofs_y = j["ofs_y"];

        return gd;
    }
}
