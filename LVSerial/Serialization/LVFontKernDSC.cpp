#include "LVFontKernDSC.h"
#include "FontRanges.h"
#include <deque>
#include "ResourceArena.h"

namespace Serialization
{
    namespace
    {
        struct KerningStorage
        {
            lv_font_fmt_txt_kern_pair_t pairs{};
            lv_font_fmt_txt_kern_classes_t classes{};
            std::vector<uint8_t> smallIds, leftClasses, rightClasses;
            std::vector<uint16_t> largeIds;
            std::vector<int8_t> values;
        };
    }

    lv_font_fmt_txt_dsc_t LVFontKernDSC::FromJSON(lv_font_fmt_txt_dsc_t& font, int classes, json record)
    {
        if (!record.is_object())
            return font;
        auto& storage = Resource<KerningStorage>();
        storage.values = record.value("Values", std::vector<int8_t>{});
        if (classes)
        {
            storage.leftClasses = record.value("leftClasses", std::vector<uint8_t>{});
            storage.rightClasses = record.value("rightClasses", std::vector<uint8_t>{});
            const auto count = GlyphCount(font);
            if (storage.leftClasses.size() < count || storage.rightClasses.size() < count)
                return font;
            storage.classes.left_class_cnt = record.value("leftCount", uint8_t{});
            storage.classes.right_class_cnt = record.value("rightCount", uint8_t{});
            if (storage.values.size() < static_cast<std::size_t>(storage.classes.left_class_cnt) * storage.classes.right_class_cnt)
                return font;
            storage.classes.left_class_mapping = storage.leftClasses.data();
            storage.classes.right_class_mapping = storage.rightClasses.data();
            storage.classes.class_pair_values = storage.values.data();
            font.kern_dsc = &storage.classes;
        }
        else
        {
            storage.pairs.glyph_ids_size = record.value("glyphIDSize", 0);
            if (storage.pairs.glyph_ids_size)
            {
                storage.largeIds = record.value("GlyphIDs", std::vector<uint16_t>{});
                storage.pairs.glyph_ids = storage.largeIds.data();
                storage.pairs.pair_cnt = static_cast<uint32_t>(std::min(storage.largeIds.size() / 2, storage.values.size()));
            }
            else
            {
                storage.smallIds = record.value("GlyphIDs", std::vector<uint8_t>{});
                storage.pairs.glyph_ids = storage.smallIds.data();
                storage.pairs.pair_cnt = static_cast<uint32_t>(std::min(storage.smallIds.size() / 2, storage.values.size()));
            }
            storage.pairs.values = storage.values.data();
            font.kern_dsc = &storage.pairs;
        }
        return font;
    }

    json LVFontKernDSC::ToJSON(lv_font_fmt_txt_dsc_t& font, int classes)
    {
        if (!font.kern_dsc)
            return nullptr;
        json record;
        if (classes)
        {
            const auto* kerning = static_cast<const lv_font_fmt_txt_kern_classes_t*>(font.kern_dsc);
            record["leftCount"] = kerning->left_class_cnt;
            record["rightCount"] = kerning->right_class_cnt;
            const auto count = GlyphCount(font);
            record["leftClasses"] = std::vector<uint8_t>(kerning->left_class_mapping, kerning->left_class_mapping + count);
            record["rightClasses"] = std::vector<uint8_t>(kerning->right_class_mapping, kerning->right_class_mapping + count);
            record["Values"] = std::vector<int8_t>(kerning->class_pair_values,
                kerning->class_pair_values + static_cast<std::size_t>(kerning->left_class_cnt) * kerning->right_class_cnt);
        }
        else
        {
            const auto* kerning = static_cast<const lv_font_fmt_txt_kern_pair_t*>(font.kern_dsc);
            record["glyphIDSize"] = kerning->glyph_ids_size;
            record["GlyphIDs"] = json::array();
            for (uint32_t i = 0; i < kerning->pair_cnt * 2; ++i)
            {
                if (kerning->glyph_ids_size)
                    record["GlyphIDs"].push_back(static_cast<const uint16_t*>(kerning->glyph_ids)[i]);
                else
                    record["GlyphIDs"].push_back(static_cast<const uint8_t*>(kerning->glyph_ids)[i]);
            }
            record["Values"] = std::vector<int8_t>(kerning->values, kerning->values + kerning->pair_cnt);
        }
        return record;
    }
}
