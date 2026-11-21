#include "LVFontCMap.h"
#include <deque>
#include "ResourceArena.h"

namespace Serialization
{
    namespace
    {
        struct CMapStorage
        {
            std::vector<uint16_t> unicode;
            std::vector<uint8_t> smallOffsets;
            std::vector<uint16_t> largeOffsets;
        };
    }

    lv_font_fmt_txt_cmap_t LVFontCMap::FromJSON(json record)
    {
        auto& storage = Resource<CMapStorage>();
        lv_font_fmt_txt_cmap_t map{};
        map.range_start = record.value("rangeStart", 0u);
        map.range_length = record.value("rangeLength", uint16_t{});
        map.glyph_id_start = record.value("glyphIDstart", uint16_t{});
        const int type = record.value("type", 0);
#if LVGL_VERSION_MAJOR >= 8
        constexpr lv_font_fmt_txt_cmap_type_t legacyTypes[] = {
            LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY, LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL,
            LV_FONT_FMT_TXT_CMAP_SPARSE_TINY, LV_FONT_FMT_TXT_CMAP_SPARSE_FULL};
        map.type = record.value("formatVersion", 6) == 6 && type >= 0 && type < 4 ?
            legacyTypes[type] : static_cast<lv_font_fmt_txt_cmap_type_t>(type);
#else
        map.type = static_cast<lv_font_fmt_txt_cmap_type_t>(type);
#endif
        const auto unicode = record.find("unicodeList");
        if (unicode != record.end() && unicode->is_array())
        {
            storage.unicode = unicode->get<std::vector<uint16_t>>();
            map.unicode_list = storage.unicode.data();
            map.list_length = static_cast<uint16_t>(storage.unicode.size());
        }
        const auto offsets = record.find("glyphIDOffsetList");
        if (offsets != record.end() && offsets->is_array())
        {
            if (map.type == LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL)
            {
                storage.smallOffsets = offsets->get<std::vector<uint8_t>>();
                map.glyph_id_ofs_list = storage.smallOffsets.data();
            }
            else if (map.type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL)
            {
                storage.largeOffsets = offsets->get<std::vector<uint16_t>>();
                map.glyph_id_ofs_list = storage.largeOffsets.data();
            }
        }
        return map;
    }

    json LVFontCMap::ToJSON(lv_font_fmt_txt_cmap_t& map)
    {
        json record{{"rangeStart", map.range_start}, {"rangeLength", map.range_length},
                    {"glyphIDstart", map.glyph_id_start}, {"type", static_cast<int>(map.type)}};
#if LVGL_VERSION_MAJOR >= 8
        record["formatVersion"] = 9;
#endif
        record["unicodeList"] = json::array();
        for (uint16_t i = 0; map.unicode_list && i < map.list_length; ++i)
            record["unicodeList"].push_back(map.unicode_list[i]);
        record["glyphIDOffsetList"] = json::array();
        if (map.glyph_id_ofs_list && map.type == LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL)
        {
            const auto* offsets = static_cast<const uint8_t*>(map.glyph_id_ofs_list);
            for (uint16_t i = 0; i < map.range_length; ++i)
                record["glyphIDOffsetList"].push_back(offsets[i]);
        }
        else if (map.glyph_id_ofs_list && map.type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL)
        {
            const auto* offsets = static_cast<const uint16_t*>(map.glyph_id_ofs_list);
            for (uint16_t i = 0; i < map.list_length; ++i)
                record["glyphIDOffsetList"].push_back(offsets[i]);
        }
        return record;
    }
}
