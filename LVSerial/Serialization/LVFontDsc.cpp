#include "LVFontDsc.h"
#include "Base64.h"
#include "FontRanges.h"
#include <deque>
#include "ResourceArena.h"

namespace Serialization
{
    void* LVFontDsc::FromJSON(json j)
    {
        struct Storage
        {
            lv_font_fmt_txt_dsc_t descriptor{};
#if LVGL_VERSION_MAJOR == 8
            lv_font_fmt_txt_glyph_cache_t cache{};
#endif
            std::vector<lv_font_fmt_txt_cmap_t> maps;
            std::vector<lv_font_fmt_txt_glyph_dsc_t> glyphs;
            std::string bitmap;
        };
        auto& storage = Resource<Storage>();
        auto& fontDsc = storage.descriptor;
#if LVGL_VERSION_MAJOR == 8
        fontDsc.cache = &storage.cache;
#endif
#if LV_VERSION_CHECK(9, 3, 0)
        fontDsc.stride = j.value("stride", 0);
#endif
        if (j["bitmapFmt"].is_number())
            fontDsc.bitmap_format = j["bitmapFmt"].get<uint16_t>();
        if (j["bpp"].is_number())
            fontDsc.bpp = j["bpp"].get<uint16_t>();
        if (j["cmapNum"].is_number())
            fontDsc.cmap_num = j["cmapNum"].get<uint16_t>();
        if (j["kernScale"].is_number())
            fontDsc.kern_scale = j["kernScale"];
        if (j["kernClasses"].is_number())
            fontDsc.kern_classes = j["kernClasses"];
#if LVGL_VERSION_MAJOR < 8
        if (j["lastGlyphID"].is_number())
            fontDsc.last_glyph_id = j["lastGlyphID"].get<uint32_t>();
        if (j["lastLetter"].is_number())
            fontDsc.last_letter = j["lastLetter"].get<uint32_t>();
#endif
        if (j["cmaps"].is_array())
        {
            auto& cmapVec = storage.maps;
            int i = 0;
            while (j["cmaps"][i].is_object())
            {
                cmapVec.push_back(LVFontCMap::FromJSON(j["cmaps"][i]));
                i++;
            }

            fontDsc.cmap_num = cmapVec.size();
            fontDsc.cmaps = cmapVec.data();
        }
        int glyphDsc = 0;
        auto& gd = storage.glyphs;
        while (j["GlyphDsc"][glyphDsc].is_object())
        {
            auto* glyph = LVFontGlyphDsc::FromJSON(j["GlyphDsc"][glyphDsc]);
            gd.push_back(*glyph);
            delete glyph;
            glyphDsc++;
        }
        fontDsc.glyph_dsc = gd.data();
        if (j["bitmap"].is_string())
        {
            storage.bitmap = base64_decode(j["bitmap"].get<std::string>());
            // LVGL's compressed bit reader looks one byte ahead even when a
            // read ends exactly on a byte boundary. Keep that byte owned by the
            // font rather than depending on another allocation's contents.
            if(fontDsc.bitmap_format)storage.bitmap.push_back('\0');
            fontDsc.glyph_bitmap = reinterpret_cast<const uint8_t*>(storage.bitmap.data());
        }
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
#if LV_VERSION_CHECK(9, 3, 0)
        j["stride"] = unsigned(fontDsc.stride);
#endif
        j["bpp"] = static_cast<int>(fontDsc.bpp);
        j["cmapNum"] = static_cast<int>(fontDsc.cmap_num);
        j["kernScale"] = static_cast<int>(fontDsc.kern_scale);
        j["kernClasses"] = static_cast<int>(fontDsc.kern_classes);
#if LVGL_VERSION_MAJOR < 8
        j["lastGlyphID"] = static_cast<uint32_t>(fontDsc.last_glyph_id);
        j["lastLetter"] = static_cast<uint32_t>(fontDsc.last_letter);
#endif
        lv_font_fmt_txt_cmap_t* ptr = (lv_font_fmt_txt_cmap_t *)fontDsc.cmaps;
        const auto glyphRange = GlyphCount(fontDsc);
        for (int i = 0; i < fontDsc.cmap_num; i++)
        {
            j["cmaps"][i] = LVFontCMap::ToJSON(*ptr++);
        }
        for (int i = 0; i < glyphRange; i++)
        {
            j["GlyphDsc"][i] =
                LVFontGlyphDsc::ToJSON(fontDsc.glyph_dsc[i]);
        }
        j["Kern"] = LVFontKernDSC::ToJSON(fontDsc, fontDsc.kern_classes);
        if (fontDsc.glyph_bitmap && fontDsc.glyph_dsc && fontDsc.bitmap_format == 0 && glyphRange > 0)
        {
            const auto& last = fontDsc.glyph_dsc[glyphRange - 1];
            auto lastBytes = (static_cast<uint64_t>(last.box_w) * last.box_h * fontDsc.bpp + 7) / 8;
#if LV_VERSION_CHECK(9, 3, 0)
            if (fontDsc.stride) {
                const auto rowBytes = (static_cast<uint64_t>(last.box_w) * fontDsc.bpp + 7) / 8;
                lastBytes = ((rowBytes + fontDsc.stride - 1) / fontDsc.stride) * fontDsc.stride * last.box_h;
            }
#endif
            const auto byteCount = static_cast<std::size_t>(last.bitmap_index + lastBytes);
            j["bitmap"] = base64_encode(fontDsc.glyph_bitmap, byteCount);
        }
        return j;
    }
}
