#include "Validation.h"
#include <Serialization/Base64.h>
#include <Serialization/ObjectSerializer.h>
#include <Serialization/CodeWidgets.h>
#include <Serialization/AdvancedKeyboard.h>
#include <functional>
#include <lvgl/lvgl.h>
#include <set>
#include <stdexcept>
namespace RuntimeValidation
{
void ValidateFeatures(const nlohmann::json &document)
{
    using json=nlohmann::json;
    const auto supported=Serialization::ObjectSerializer::SupportedTypes();
    const std::set<std::string> enabled(supported.begin(),supported.end());
    const std::pair<const char*,const char*> kinds[]={
        {"arc","arc"},{"bar","bar"},{"button","button"},{"btnm","buttonMatrix"},{"cal","calendar"},
        {"canvas","canvas"},{"cb","checkbox"},{"img","image"},{"text","label"},{"dropdown","dropdown"},
        {"roller","roller"},{"slider","slider"},{"spinbox","spinbox"},{"switch","switch"},
        {"textarea","textarea"},{"led","led"},{"line","line"},{"spinner","spinner"},
        {"chart","chart"},{"table","table"},{"scale","scale"},{"keyboard","keyboard"},
        {"tabview","tabview"},{"tileview","tileview"},{"window","window"},{"messagebox","messagebox"},
        {"imagebutton","imagebutton"},{"textBox","textBox"},{"layerView","layerView"},
        {"barcode","barcode"},{"qrcode","qrcode"},{"advancedKeyboard","advancedKeyboard"}};
    auto walk=[&](const auto& self,const json& value)->void {
        if(value.is_object()) {
            if(value.contains("base") && value["base"].is_object()) {
                for(const auto& kind:kinds)
                    if(value.contains(kind.first) && !enabled.count(kind.second))
                        throw std::invalid_argument(std::string("Widget not enabled in this runtime: ")+kind.second);
                for(const auto* code:{"barcode","qrcode"})
                    if(value.contains(code)) Serialization::CodeWidgets::Normalize(code,value[code]);
                if(value.contains("advancedKeyboard"))
                    Serialization::AdvancedKeyboard::Normalize(value["advancedKeyboard"]);
            }
            for(const auto& child:value.items())self(self,child.value());
        } else if(value.is_array())for(const auto& child:value)self(self,child);
    };
    walk(walk,document);
}

void ValidateFonts(const nlohmann::json &document)
{
    using json = nlohmann::json;
    std::set<std::string> checked;
    auto validate = [&](const json &font) {
        if (!font.at("lineHeight").is_number_integer() || font.at("lineHeight").get<int64_t>() < 1 ||
            font.at("lineHeight").get<int64_t>() > 32767)
            throw std::invalid_argument("Invalid font line height");
        const auto &d = font.at("dsc");
        if (!d.is_object() || !d.contains("bitmap") || !d["bitmap"].is_string() || !d.contains("GlyphDsc") ||
            !d["GlyphDsc"].is_array() || d["GlyphDsc"].empty())
            throw std::invalid_argument("Font is missing glyph descriptors or bitmap data");
        int bpp = d.value("bpp", 4), format = d.value("bitmapFmt", 0), stride = d.value("stride", 0);
        if ((bpp != 1 && bpp != 2 && bpp != 3 && bpp != 4 && bpp != 8) || format < 0 || format > 2 ||
            stride < 0 || stride > 255)
            throw std::invalid_argument("Invalid font bitmap format");
        if (bpp == 3 && format == 0)
            throw std::invalid_argument("Plain 3-bpp fonts cannot be rendered by LVGL; use a normalized "
                                        "project font or compressed font");
#if !LV_USE_FONT_COMPRESSED
        if (format)
            throw std::invalid_argument("Enable LV_USE_FONT_COMPRESSED for this font");
#endif
        auto bitmap = base64_decode(d["bitmap"]);
        auto glyphCount = d["GlyphDsc"].size();
        for (const auto &glyph : d["GlyphDsc"])
        {
            if (!glyph.is_object())
                throw std::invalid_argument("Invalid font glyph");
            auto w = glyph.value("box_w", uint64_t(0)), h = glyph.value("box_h", uint64_t(0)),
                 offset = glyph.value("bitmapIdx", uint64_t(0)), advance = glyph.value("adv_w", uint64_t(0));
            lv_font_fmt_txt_glyph_dsc_t probe{};
            probe.box_w = (uint32_t)w;
            probe.box_h = (uint32_t)h;
            probe.bitmap_index = (uint32_t)offset;
            probe.adv_w = (uint32_t)advance;
            if (probe.box_w != w || probe.box_h != h || probe.bitmap_index != offset ||
                probe.adv_w != advance)
                throw std::invalid_argument("Font glyph metrics exceed the native descriptor range");
            uint64_t length =
                stride ? ((w * bpp + 7) / 8 + stride - 1) / stride * stride * h : (w * h * bpp + 7) / 8;
            if (offset > bitmap.size() || (!format && length > bitmap.size() - offset) ||
                (format && w && h && offset == bitmap.size()))
                throw std::invalid_argument("Font glyph bitmap is truncated");
        }
        for (const auto &map : d.value("cmaps", json::array()))
        {
            auto start = map.at("glyphIDstart").get<uint64_t>(),
                 length = map.at("rangeLength").get<uint64_t>();
            if (!length || length > 65535 || start >= glyphCount)
                throw std::invalid_argument("Invalid font cmap range");
            int type = map.value("type", 0);
            if (map.value("formatVersion", 6) == 6)
            {
                const int legacy[] = {LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY, LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL,
                                      LV_FONT_FMT_TXT_CMAP_SPARSE_TINY, LV_FONT_FMT_TXT_CMAP_SPARSE_FULL};
                if (type < 0 || type > 3)
                    throw std::invalid_argument("Invalid font cmap type");
                type = legacy[type];
            }
            auto unicode = map.value("unicodeList", json::array()),
                 offsets = map.value("glyphIDOffsetList", json::array());
            if (type == LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY)
            {
                if (start + length > glyphCount)
                    throw std::invalid_argument("Font cmap references a missing glyph");
            }
            else if (type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY || type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL)
            {
                uint64_t previous = 0;
                for (size_t i = 0; i < unicode.size(); ++i)
                {
                    auto code = unicode[i].get<uint64_t>();
                    if (code >= length || (i && code <= previous))
                        throw std::invalid_argument("Invalid sparse cmap ordering");
                    previous = code;
                }
                if (type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY && start + unicode.size() > glyphCount)
                    throw std::invalid_argument("Sparse cmap references a missing glyph");
                if (type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL && offsets.size() < unicode.size())
                    throw std::invalid_argument("Font cmap offsets are missing");
            }
            else if (type == LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL)
            {
                if (offsets.size() < length)
                    throw std::invalid_argument("Font cmap offsets are missing");
            }
            else
                throw std::invalid_argument("Invalid font cmap type");
            for (const auto &offset : offsets)
            {
                auto index = offset.get<uint64_t>();
                if (start + index >= glyphCount || (type == LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL && index > 255))
                    throw std::invalid_argument("Font cmap offset is outside its glyph table");
            }
        }
    };
    std::function<void(const json &)> walk = [&](const json &value) {
        if (value.is_array())
        {
            for (const auto &child : value)
                walk(child);
            return;
        }
        if (!value.is_object())
            return;
        if (value.contains("lineHeight") && value.contains("dsc"))
        {
            auto signature = value.dump();
            if (checked.insert(signature).second)
                validate(value);
            return;
        }
        for (const auto &child : value.items())
            walk(child.value());
    };
    walk(document);
}
} // namespace RuntimeValidation
