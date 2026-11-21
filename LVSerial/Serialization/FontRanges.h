#pragma once

#include <lvgl/lvgl.h>
#include <algorithm>

namespace Serialization
{
    inline uint32_t GlyphCount(const lv_font_fmt_txt_dsc_t& font)
    {
        uint32_t count = 1;
        for (uint16_t index = 0; font.cmaps && index < font.cmap_num; ++index)
        {
            const auto& map = font.cmaps[index];
            uint32_t last = map.glyph_id_start;
            if (map.type == LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY)
                last += map.range_length;
            else if (map.type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY)
                last += map.list_length;
            else if (map.type == LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL && map.glyph_id_ofs_list)
            {
                const auto* offsets = static_cast<const uint8_t*>(map.glyph_id_ofs_list);
                for (uint16_t i = 0; i < map.range_length; ++i)
                    last = std::max(last, static_cast<uint32_t>(map.glyph_id_start + offsets[i] + 1));
            }
            else if (map.type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL && map.glyph_id_ofs_list)
            {
                const auto* offsets = static_cast<const uint16_t*>(map.glyph_id_ofs_list);
                for (uint16_t i = 0; i < map.list_length; ++i)
                    last = std::max(last, static_cast<uint32_t>(map.glyph_id_start + offsets[i] + 1));
            }
            count = std::max(count, last);
        }
        return count;
    }
}
