#include "LVFont.h"
#include <deque>
#include "ResourceArena.h"
#include <map>

namespace Serialization
{
    json LVFont::ToJSON(const lv_font_t& font)
    {
        // Restored fonts are immutable. Their original portable data includes
        // the exact compressed bitmap length, which LVGL's descriptor alone
        // does not expose. Reuse that data without probing beyond its buffer.
        for (const auto& entry : Resources().fonts)
            if (entry.second == &font) return json::parse(entry.first);
        json j;
        j["baseLine"] = font.base_line;
        j["lineHeight"] = font.line_height;
#if LVGL_VERSION_MAJOR >= 9
#if LV_VERSION_CHECK(9, 6, 0)
        j["capHeight"] = font.cap_height;
        j["xHeight"] = font.x_height;
#endif
        j["kerning"] = unsigned(font.kerning);
#endif
        j["subpx"] = unsigned(font.subpx);
        j["underlinePosition"] = font.underline_position;
        j["underlineThickness"] = font.underline_thickness;
        if (font.dsc && font.get_glyph_dsc == lv_font_get_glyph_dsc_fmt_txt)
        {
            auto dsc = *static_cast<const lv_font_fmt_txt_dsc_t*>(font.dsc);
            j["dsc"] = LVFontDsc::ToJSON(dsc, font);
        }
        return j;
    }

    lv_font_t& LVFont::FromJSON(json j)
    {
        if(j.is_object() && j.size()==1 && j.contains("projectFont")) {
            auto id=j.at("projectFont").get<std::string>();auto found=Resources().fontRefs.find(id);
            if(found==Resources().fontRefs.end())throw std::invalid_argument("Unknown scoped project font: "+id);
            return *found->second;
        }
        // Bitmap fonts are immutable resources. Reuse identical restored fonts
        // across widget styles and the project catalog instead of allocating a
        // bitmap copy for every widget that selects the same library entry.
        auto& resources=Resources().fonts;
        const auto signature=j.dump();
        const auto existing=resources.find(signature);if(existing!=resources.end())return *existing->second;
        auto& newFont = Resource<lv_font_t>(*LV_FONT_DEFAULT);
        if (j["baseLine"].is_number())
            newFont.base_line = j["baseLine"].get<int>();
        if (j["lineHeight"].is_number())
            newFont.line_height = j["lineHeight"].get<int>();
#if LVGL_VERSION_MAJOR >= 9
#if LV_VERSION_CHECK(9, 3, 0)
        newFont.static_bitmap = !j.contains("dsc") || j["dsc"].value("bitmapFmt",0)==0;
#endif
#if LV_VERSION_CHECK(9, 6, 0)
        newFont.cap_height = j.value("capHeight", 0);
        newFont.x_height = j.value("xHeight", 0);
#endif
        newFont.kerning = j.value("kerning", 0);
#endif
        newFont.subpx = j.value("subpx", 0);
        newFont.underline_position = j.value("underlinePosition", 0);
        newFont.underline_thickness = j.value("underlineThickness", 0);
        if (j["dsc"].is_object() && j["dsc"].contains("bitmap"))
        {
            auto* descriptor = LVFontDsc::FromJSON(j["dsc"]);
            if (descriptor)
                newFont.dsc = descriptor;
        }
        resources.emplace(signature,&newFont);return newFont;
    }
}
