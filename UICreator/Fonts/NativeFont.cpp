// Native FreeType rasterization and LVGL font export.
// LVGL descriptor/dump layout adapted from lv_font_conv, Copyright (c) 2018
// authors, MIT license: ../../../3rdParty/lv_font_conv/LICENSE.
#include "NativeFont.h"
#include "NativeFontKerning.h"
#include "../../3rdParty/JSON/json.hpp"
#include <NativePng.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <set>

namespace FontConversion::Native {
namespace {
using json = nlohmann::json;
void CheckCancel(const std::atomic<bool>* cancel) {
    if (cancel && cancel->load()) throw std::runtime_error("Conversion cancelled");
}
uint32_t Point(std::string text) {
    const auto first = text.find_first_not_of(" \t"), last = text.find_last_not_of(" \t");
    if (first == std::string::npos) throw std::invalid_argument("Empty Unicode code point");
    text = text.substr(first, last - first + 1);
    const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const auto digits = hex ? text.substr(2) : text;
    if (digits.empty() || digits.find_first_not_of(hex ? "0123456789abcdefABCDEF" : "0123456789") != std::string::npos)
        throw std::invalid_argument("Invalid Unicode code point: " + text);
    size_t consumed = 0; unsigned long value;
    try { value = std::stoul(digits, &consumed, hex ? 16 : 10); }
    catch (...) { throw std::invalid_argument("Invalid Unicode code point: " + text); }
    if (consumed != digits.size() || value > 0x10ffff) throw std::invalid_argument("Code point exceeds Unicode range");
    return static_cast<uint32_t>(value);
}
std::vector<uint32_t> Symbols(const std::string& text) {
    std::vector<uint32_t> result;
    for (size_t at = 0; at < text.size();) {
        const auto byte = static_cast<uint8_t>(text[at++]);
        uint32_t point = byte; unsigned continuation = 0, minimum = 0;
        if (byte < 0x80) {}
        else if (byte >= 0xc2 && byte <= 0xdf) { point = byte & 31; continuation = 1; minimum = 0x80; }
        else if (byte >= 0xe0 && byte <= 0xef) { point = byte & 15; continuation = 2; minimum = 0x800; }
        else if (byte >= 0xf0 && byte <= 0xf4) { point = byte & 7; continuation = 3; minimum = 0x10000; }
        else throw std::invalid_argument("Symbols contain invalid UTF-8");
        for (unsigned n = 0; n < continuation; ++n) {
            if (at == text.size() || (static_cast<uint8_t>(text[at]) & 0xc0) != 0x80) throw std::invalid_argument("Symbols contain invalid UTF-8");
            point = (point << 6) | (static_cast<uint8_t>(text[at++]) & 63);
        }
        if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) throw std::invalid_argument("Symbols contain invalid UTF-8");
        result.push_back(point);
    }
    return result;
}
struct Selection { size_t face; uint32_t code; };
std::vector<std::pair<uint32_t, uint32_t>> Ranges(const std::string& text) {
    std::vector<std::pair<uint32_t, uint32_t>> result;
    if (text.empty()) return result;
    size_t position = 0;
    while (position <= text.size()) {
        const auto comma = text.find(',', position);
        const auto item = text.substr(position, comma == std::string::npos ? std::string::npos : comma - position);
        const auto arrow = item.find("=>");
        const auto range = item.substr(0, arrow);
        const auto dash = range.find('-');
        const auto first = Point(range.substr(0, dash));
        const auto last = dash == std::string::npos ? first : Point(range.substr(dash + 1));
        const auto mapped = arrow == std::string::npos ? first : Point(item.substr(arrow + 2));
        if (last < first || uint64_t(mapped) + last - first > 0x10ffff) throw std::invalid_argument("Invalid Unicode range: " + item);
        for (uint32_t code = first; code <= last; ++code)
            if (!(code >= 0xd800 && code <= 0xdfff)) result.emplace_back(code, mapped + code - first);
        if (comma == std::string::npos) break;
        position = comma + 1;
    }
    return result;
}
class LegacyKernCoverage {
    bool known = false;
    std::set<FT_UInt> left;
public:
    explicit LegacyKernCoverage(FT_Face face) {
        FT_ULong size = 0;
        if (FT_Load_Sfnt_Table(face, FT_MAKE_TAG('k','e','r','n'), 0, nullptr, &size) || size < 4 || size > 64u * 1024u * 1024u) return;
        std::vector<uint8_t> data(size);
        if (FT_Load_Sfnt_Table(face, FT_MAKE_TAG('k','e','r','n'), 0, data.data(), &size)) return;
        auto u16 = [&](size_t at) -> uint16_t { if (at > data.size() || data.size() - at < 2) throw std::runtime_error("Invalid legacy kerning table"); return uint16_t((uint16_t(data[at]) << 8) | data[at + 1]); };
        if (u16(0) != 0) return; // Non-SFNT/Apple variants retain FreeType's full fallback.
        size_t at = 4;
        for (unsigned table = 0; table < u16(2); ++table) {
            const auto length = u16(at + 2), coverage = u16(at + 4);
            if (length < 6 || at > data.size() || data.size() - at < length) throw std::runtime_error("Invalid legacy kerning subtable");
            if ((coverage >> 8) != 0) return;
            if (length < 14 || size_t(u16(at + 6)) * 6 > length - 14) throw std::runtime_error("Invalid legacy kerning pairs");
            for (unsigned pair = 0; pair < u16(at + 6); ++pair) left.insert(u16(at + 14 + size_t(pair) * 6));
            at += length;
        }
        known = true;
    }
    bool HasLeft(FT_UInt glyph) const { return !known || left.count(glyph) != 0; }
};
struct Face {
    std::vector<uint8_t> bytes;
    FT_Face value = nullptr;
    std::unique_ptr<GposKerning> gpos;
    std::unique_ptr<LegacyKernCoverage> legacyCoverage;
    ~Face() { if (value) FT_Done_Face(value); }
};
struct Library {
    FT_Library value = nullptr;
    Library() { if (FT_Init_FreeType(&value)) throw std::runtime_error("Cannot initialize FreeType"); }
    ~Library() { FT_Done_FreeType(value); }
};
std::vector<uint8_t> Bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Cannot read font: " + path.u8string());
    auto size = stream.tellg();
    if (size <= 0 || size > 512 * 1024 * 1024) throw std::runtime_error("Font file is empty or too large");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    stream.seekg(0); stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!stream) throw std::runtime_error("Cannot read font: " + path.u8string());
    return bytes;
}
template<class T> void Array(std::ostringstream& out, const char* type, const std::string& name, const std::vector<T>& values, bool bitmap = false) {
    out << "static " << (bitmap ? "LV_ATTRIBUTE_LARGE_CONST " : "") << "const " << type << " " << name << "[] = {\n    ";
    if (values.empty()) out << "0";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) out << (i % 16 == 0 ? ",\n    " : ", ");
        out << static_cast<int64_t>(values[i]);
    }
    out << "\n};\n\n";
}
struct Map { size_t first, last; bool sparse; };
std::vector<Map> Maps(const FontData& font) {
    std::vector<Map> maps;
    size_t first = 0;
    while (first < font.glyphs.size()) {
        size_t last = first + 1;
        while (last < font.glyphs.size() && font.glyphs[last].code - font.glyphs[first].code < 65535) ++last;
        std::vector<Map> runs;
        size_t run = first;
        for (size_t i = first + 1; i <= last; ++i) if (i == last || font.glyphs[i].code != font.glyphs[i - 1].code + 1) { runs.push_back({run, i, false}); run = i; }
        if (runs.size() * 32 <= 32 + (last - first) * 2) maps.insert(maps.end(), runs.begin(), runs.end());
        else maps.push_back({first, last, true});
        first = last;
    }
    return maps;
}
}

FontData Rasterize(const Options& options, const std::atomic<bool>* cancel) {
    CheckCancel(cancel);
    Library library;
    std::vector<std::unique_ptr<Face>> faces;
    std::map<uint32_t, Selection> selected;
    FontData font; font.size = options.size;
    font.ascent = std::numeric_limits<int>::min(); font.descent = std::numeric_limits<int>::max();
    for (size_t sourceIndex = 0; sourceIndex < options.sources.size(); ++sourceIndex) {
        CheckCancel(cancel);
        const auto& source = options.sources[sourceIndex];
        auto face = std::make_unique<Face>(); face->bytes = Bytes(source.path);
        const auto error = FT_New_Memory_Face(library.value, face->bytes.data(), static_cast<FT_Long>(face->bytes.size()), 0, &face->value);
        if (error) throw std::runtime_error("FreeType cannot open font " + source.path.u8string() + " (error " + std::to_string(error) + ")");
        if (FT_Select_Charmap(face->value, FT_ENCODING_UNICODE) && !face->value->charmap) throw std::runtime_error("Font has no Unicode character map");
        if (FT_Set_Pixel_Sizes(face->value, 0, options.size)) throw std::runtime_error("Cannot set the requested font size");
        const auto scale = double(options.size) / (face->value->units_per_EM ? face->value->units_per_EM : options.size);
        font.ascent = (std::max)(font.ascent, static_cast<int>(std::ceil(face->value->ascender * scale)));
        font.descent = (std::min)(font.descent, static_cast<int>(std::floor(face->value->descender * scale)));
        if (sourceIndex == 0) {
            auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face->value, ft_sfnt_os2));
            font.typoAscent = os2 ? int(std::lround(os2->sTypoAscender * scale)) : font.ascent;
            font.typoDescent = os2 ? int(std::lround(os2->sTypoDescender * scale)) : font.descent;
            font.typoLineGap = os2 ? int(std::lround(os2->sTypoLineGap * scale)) : 0;
            font.capHeight = os2 && os2->version >= 2 && os2->version != 0xffff ? int(std::lround(os2->sCapHeight * scale)) : 0;
            font.xHeight = os2 && os2->version >= 2 && os2->version != 0xffff ? int(std::lround(os2->sxHeight * scale)) : 0;
            font.underlinePosition = int(std::lround(face->value->underline_position * scale));
            font.underlineThickness = (std::max)(1, int(std::lround(face->value->underline_thickness * scale)));
        }
        auto ranges = Ranges(source.ranges);
        bool anyRange = false;
        for (const auto& entry : ranges) {
            if (FT_Get_Char_Index(face->value, entry.first)) anyRange = true;
            selected[entry.second] = {sourceIndex, entry.first};
        }
        if (!ranges.empty() && !anyRange) throw std::invalid_argument("Source font contains no characters from its Unicode ranges");
        auto symbols = Symbols(source.symbols); bool anySymbols = false;
        for (const auto code : symbols) { if (FT_Get_Char_Index(face->value, code)) anySymbols = true; selected[code] = {sourceIndex, code}; }
        if (!symbols.empty() && !anySymbols) throw std::invalid_argument("Source font contains none of the selected symbols");
        if (options.kerning) {
            face->gpos = std::make_unique<GposKerning>(face->value);
            if (!face->gpos->Present() && FT_HAS_KERNING(face->value)) face->legacyCoverage = std::make_unique<LegacyKernCoverage>(face->value);
        }
        faces.push_back(std::move(face));
    }
    std::vector<Selection> origins;
    std::vector<FT_UInt> indices;
    size_t totalPixels = 0;
    for (const auto& selection : selected) {
        CheckCancel(cancel);
        auto& face = *faces[selection.second.face];
        const auto glyphIndex = FT_Get_Char_Index(face.value, selection.second.code);
        if (!glyphIndex) continue;
        auto flags = FT_LOAD_RENDER;
        if (options.smoothing == 1) flags |= FT_LOAD_TARGET_LCD;
        else if (options.smoothing == 2) flags |= FT_LOAD_TARGET_LCD_V;
        else if (options.bpp == 1) flags |= FT_LOAD_TARGET_MONO;
        else flags |= options.sources[selection.second.face].hinting == 2 ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_LIGHT;
        flags |= options.sources[selection.second.face].hinting == 1 ? FT_LOAD_NO_AUTOHINT : FT_LOAD_FORCE_AUTOHINT;
        if (options.colorInfo) flags |= FT_LOAD_COLOR;
        if (FT_Load_Glyph(face.value, glyphIndex, flags)) throw std::runtime_error("Cannot rasterize Unicode code point " + std::to_string(selection.first));
        const auto* slot = face.value->glyph; const auto& bitmap = slot->bitmap;
        Glyph glyph; glyph.code = selection.first;
        glyph.advance16 = int32_t(std::lround(double(slot->linearHoriAdvance) / 4096.0));
        // Bitmap-only faces report no linear advance.
        if (!glyph.advance16 && slot->advance.x) glyph.advance16 = int32_t(std::lround(double(slot->advance.x) / 4.0));
        glyph.x = slot->bitmap_left; glyph.y = slot->bitmap_top - int(bitmap.rows);
        glyph.width = bitmap.width; glyph.height = bitmap.rows;
        const uint64_t area = uint64_t(glyph.width) * glyph.height;
        if (area > 128u * 1024u * 1024u || totalPixels + area > 512u * 1024u * 1024u) throw std::runtime_error("Selected font bitmaps exceed the 512 MiB conversion limit; reduce font size or character ranges");
        glyph.pixels.resize(static_cast<size_t>(area)); totalPixels += static_cast<size_t>(area);
        for (unsigned y = 0; y < bitmap.rows; ++y) {
            const auto* row = bitmap.buffer + (bitmap.pitch >= 0 ? size_t(y) * bitmap.pitch : size_t(bitmap.rows - 1 - y) * -bitmap.pitch);
            for (unsigned x = 0; x < bitmap.width; ++x) {
                uint8_t alpha;
                if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) alpha = (row[x / 8] & (0x80 >> (x % 8))) ? 255 : 0;
                else if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
                    const auto gray = (299u * row[x * 4 + 2] + 587u * row[x * 4 + 1] + 114u * row[x * 4] + 500) / 1000;
                    alpha = static_cast<uint8_t>((255u - gray) * row[x * 4 + 3] / 255u);
                } else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY || bitmap.pixel_mode == FT_PIXEL_MODE_LCD || bitmap.pixel_mode == FT_PIXEL_MODE_LCD_V) alpha = row[x];
                else throw std::runtime_error("Unsupported FreeType bitmap pixel format");
                glyph.pixels[size_t(y) * bitmap.width + x] = alpha;
            }
        }
        if (options.format == "dump") {
            GlyphDebug debug;
            const auto& m = slot->metrics;
            debug.metrics26 = {int32_t(m.width), int32_t(m.height), int32_t(m.horiBearingX), int32_t(m.horiBearingY), int32_t(m.horiAdvance), int32_t(m.vertBearingX), int32_t(m.vertBearingY), int32_t(m.vertAdvance)};
            debug.linearHori16 = int32_t(slot->linearHoriAdvance); debug.linearVert16 = int32_t(slot->linearVertAdvance);
            debug.advanceX26 = int32_t(slot->advance.x); debug.advanceY26 = int32_t(slot->advance.y);
            debug.lsb26 = int32_t(slot->lsb_delta); debug.rsb26 = int32_t(slot->rsb_delta);
            debug.pitch = bitmap.pitch; debug.numGrays = bitmap.num_grays; debug.pixelMode = bitmap.pixel_mode; debug.paletteMode = bitmap.palette_mode;
            font.debug.push_back(debug);
        }
        font.glyphs.push_back(std::move(glyph)); origins.push_back(selection.second); indices.push_back(glyphIndex);
        if (font.glyphs.size() > 65535) throw std::invalid_argument("An LVGL font supports at most 65535 selected glyphs");
    }
    if (font.glyphs.empty()) throw std::invalid_argument("The selected ranges contain no available glyphs");
    if (font.ascent - font.descent <= 0 || font.ascent - font.descent > 65535 || -font.descent < 0 || -font.descent > 65535)
        throw std::invalid_argument("Font line metrics exceed LVGL's supported range");
    if (options.kerning) for (size_t left = 0; left < font.glyphs.size(); ++left) {
        CheckCancel(cancel);
        auto& face = *faces[origins[left].face];
        if (face.gpos->Present() ? !face.gpos->HasPairs() : !FT_HAS_KERNING(face.value)) continue;
        if (face.gpos->Present() ? !face.gpos->HasLeft(static_cast<uint16_t>(indices[left])) : !face.legacyCoverage->HasLeft(indices[left])) continue;
        const auto scale16 = double(options.size) * 16 / (face.value->units_per_EM ? face.value->units_per_EM : options.size);
        for (size_t right = 0; right < font.glyphs.size(); ++right) {
            if ((right & 255) == 0) CheckCancel(cancel);
            if (origins[left].face != origins[right].face) continue;
            int kern = 0;
            if (face.gpos->Present()) kern = face.gpos->Pair(static_cast<uint16_t>(indices[left]), static_cast<uint16_t>(indices[right]));
            else { FT_Vector vector{}; if (!FT_Get_Kerning(face.value, indices[left], indices[right], FT_KERNING_UNSCALED, &vector)) kern = static_cast<int>(vector.x); }
            const auto value = int32_t(std::lround(kern * scale16));
            if (value) {
                if (font.kerning.size() >= 16u * 1024u * 1024u) throw std::runtime_error("Selected kerning pairs exceed the conversion memory limit; reduce character ranges or disable kerning");
                font.kerning.push_back({static_cast<uint16_t>(left + 1), static_cast<uint16_t>(right + 1), value});
            }
        }
    }
    return font;
}

std::string CSource(const FontData& font, const Options& options) {
    if (options.bpp == 3 && !options.compress) throw std::invalid_argument("LVGL C supports 3 bpp only with compression enabled");
    std::ostringstream out;
    out << "/* Native LVGL font: " << options.size << " px, " << options.bpp << " bpp. Generated by LVGL UI Creator. */\n#include \"" << options.include << "\"\n\n";
    if (options.stride) out << "#if !LV_VERSION_CHECK(9, 3, 0)\n#error \"Font row stride requires LVGL 9.3 or later\"\n#endif\n";
    std::vector<uint8_t> bitmap;
    std::vector<size_t> offsets;
    bool large = false;
    for (const auto& glyph : font.glyphs) {
        offsets.push_back(bitmap.size()); auto encoded = Bitmap(glyph, options); bitmap.insert(bitmap.end(), encoded.begin(), encoded.end());
        large |= glyph.width > 255 || glyph.height > 255 || glyph.x < -128 || glyph.x > 127 || glyph.y < -128 || glyph.y > 127 || glyph.advance16 > 4095;
    }
    large |= bitmap.size() > 0xfffff;
    if (large) out << "#if !LV_FONT_FMT_TXT_LARGE\n#error \"Enable LV_FONT_FMT_TXT_LARGE for this font\"\n#endif\n";
    Array(out, "uint8_t", "glyph_bitmap", bitmap, true);
    out << "static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n    {0, 0, 0, 0, 0, 0}";
    for (size_t i = 0; i < font.glyphs.size(); ++i) {
        const auto& g = font.glyphs[i];
        if (g.advance16 < 0 || g.advance16 > 65535 || g.width > 65535 || g.height > 65535 || g.x < -32768 || g.x > 32767 || g.y < -32768 || g.y > 32767) throw std::invalid_argument("Glyph metrics exceed LVGL's large font descriptor");
        out << ",\n    {.bitmap_index = " << offsets[i] << ", .adv_w = " << g.advance16 << ", .box_w = " << g.width << ", .box_h = " << g.height << ", .ofs_x = " << g.x << ", .ofs_y = " << g.y << "}";
    }
    out << "\n};\n\n";
    const auto maps = Maps(font);
    for (size_t i = 0; i < maps.size(); ++i) if (maps[i].sparse) {
        std::vector<uint16_t> codes;
        for (size_t g = maps[i].first; g < maps[i].last; ++g) codes.push_back(static_cast<uint16_t>(font.glyphs[g].code - font.glyphs[maps[i].first].code));
        Array(out, "uint16_t", "unicode_" + std::to_string(i), codes);
    }
    out << "static const lv_font_fmt_txt_cmap_t cmaps[] = {\n";
    for (size_t i = 0; i < maps.size(); ++i) {
        const auto& map = maps[i]; const auto start = font.glyphs[map.first].code;
        out << (i ? ",\n" : "") << "    {.range_start = " << start << ", .range_length = " << font.glyphs[map.last - 1].code - start + 1 << ", .glyph_id_start = " << map.first + 1
            << ", .unicode_list = " << (map.sparse ? "unicode_" + std::to_string(i) : "NULL") << ", .glyph_id_ofs_list = NULL, .list_length = " << (map.sparse ? map.last - map.first : 0)
            << ", .type = " << (map.sparse ? "LV_FONT_FMT_TXT_CMAP_SPARSE_TINY" : "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY") << "}";
    }
    out << "\n};\n\n";
    const auto kern = Kerning(font, options);
    const bool hasKern = options.kerning && !font.kerning.empty();
    if (hasKern) {
        if (kern.classes) {
            Array(out, "uint8_t", "kern_left", kern.leftMapping); Array(out, "uint8_t", "kern_right", kern.rightMapping); Array(out, "int8_t", "kern_values", kern.values);
            out << "static const lv_font_fmt_txt_kern_classes_t kern_dsc = {.class_pair_values = kern_values, .left_class_mapping = kern_left, .right_class_mapping = kern_right, .left_class_cnt = " << int(kern.leftClasses) << ", .right_class_cnt = " << int(kern.rightClasses) << "};\n\n";
        } else {
            std::vector<uint16_t> ids; for (const auto& pair : font.kerning) { ids.push_back(pair.left); ids.push_back(pair.right); }
            if (font.glyphs.size() <= 255) { std::vector<uint8_t> small; small.reserve(ids.size()); for (auto id : ids) small.push_back(static_cast<uint8_t>(id)); Array(out, "uint8_t", "kern_ids", small); }
            else Array(out, "uint16_t", "kern_ids", ids);
            Array(out, "int8_t", "kern_values", kern.values);
            out << "static const lv_font_fmt_txt_kern_pair_t kern_dsc = {.glyph_ids = kern_ids, .values = kern_values, .pair_cnt = " << font.kerning.size() << ", .glyph_ids_size = " << (font.glyphs.size() <= 255 ? 0 : 1) << "};\n\n";
        }
    }
    out << "#if LVGL_VERSION_MAJOR == 8\nstatic lv_font_fmt_txt_glyph_cache_t cache;\n#endif\nstatic const lv_font_fmt_txt_dsc_t font_dsc = {\n    .glyph_bitmap = glyph_bitmap, .glyph_dsc = glyph_dsc, .cmaps = cmaps,\n    .kern_dsc = " << (hasKern ? "&kern_dsc" : "NULL") << ", .kern_scale = " << (hasKern ? kern.scale16 : 0) << ", .cmap_num = " << maps.size() << ", .bpp = " << options.bpp
        << ", .kern_classes = " << (hasKern && kern.classes ? 1 : 0) << ", .bitmap_format = " << Compression(options) << ",\n#if LVGL_VERSION_MAJOR == 8\n    .cache = &cache,\n#endif\n";
    if (options.stride) out << "    .stride = " << options.stride << ",\n";
    out << "};\n\n";
    if (!options.fallback.empty()) out << "extern const lv_font_t " << options.fallback << ";\n";
    out << "const lv_font_t " << options.name << " = {\n    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n    .line_height = " << font.ascent - font.descent << ", .base_line = " << -font.descent << ",\n#if LV_VERSION_CHECK(9, 6, 0)\n    .cap_height = " << font.capHeight << ", .x_height = " << font.xHeight << ",\n#endif\n    .subpx = " << (options.smoothing == 1 ? "LV_FONT_SUBPX_HOR" : options.smoothing == 2 ? "LV_FONT_SUBPX_VER" : "LV_FONT_SUBPX_NONE") << ",\n    .underline_position = " << font.underlinePosition << ", .underline_thickness = " << font.underlineThickness
        << ",\n#if LV_VERSION_CHECK(9, 3, 0)\n    .static_bitmap = " << (Compression(options) ? 0 : 1) << ",\n#endif\n    .dsc = &font_dsc, .fallback = " << (options.fallback.empty() ? "NULL" : "&" + options.fallback) << ", .user_data = NULL\n};\n";
    return out.str();
}

std::vector<std::pair<std::filesystem::path, std::vector<uint8_t>>> Dump(const FontData& font, const Options& options, const std::atomic<bool>* cancel) {
    std::vector<std::pair<std::filesystem::path, std::vector<uint8_t>>> output;
    json info = {{"size", font.size}, {"ascent", font.ascent}, {"descent", font.descent}, {"typoAscent", font.typoAscent}, {"typoDescent", font.typoDescent}, {"typoLineGap", font.typoLineGap}, {"capHeight", font.capHeight}, {"xHeight", font.xHeight}, {"underlinePosition", font.underlinePosition}, {"underlineThickness", font.underlineThickness}, {"glyphs", json::array()}};
    for (size_t index = 0; index < font.glyphs.size(); ++index) {
        CheckCancel(cancel);
        const auto& glyph = font.glyphs[index];
        const auto minX = glyph.x, maxX = (std::max)(glyph.x + int(glyph.width) - 1, glyph.x);
        const auto minY = (std::min)(glyph.y, font.typoDescent), maxY = (std::max)(glyph.y + int(glyph.height) - 1, font.typoAscent);
        const auto width = maxX - minX + 1, height = maxY - minY + 1;
        if (width <= 0 || height <= 0 || uint64_t(width) * height > 128u * 1024u * 1024u) throw std::invalid_argument("Glyph dump exceeds the image size limit");
        std::vector<uint8_t> pixels(size_t(width) * height * 4);
        const auto advance = int(std::lround(glyph.advance16 / 16.0));
        for (int y = maxY; y >= minY; --y) for (int x = minX; x <= maxX; ++x) {
            unsigned value = 0;
            if (x >= glyph.x && x < glyph.x + int(glyph.width) && y >= glyph.y && y < glyph.y + int(glyph.height)) value = glyph.pixels[size_t(glyph.height - 1 - (y - glyph.y)) * glyph.width + x - glyph.x];
            const auto max = (1u << options.bpp) - 1;
            value = ((value >> (8 - options.bpp)) * 255u + max / 2) / max;
            const auto pos = (size_t(maxY - y) * width + x - minX) * 4;
            const bool outside = x < 0 || x >= advance || y < font.typoDescent || y > font.typoAscent;
            pixels[pos] = static_cast<uint8_t>(255 - value); pixels[pos + 1] = static_cast<uint8_t>((255 - value) * (outside ? 127 : 255) / 255); pixels[pos + 2] = static_cast<uint8_t>((255 - value) * (outside ? 184 : 255) / 255); pixels[pos + 3] = 255;
        }
        auto png = NativePng::Encode(pixels.data(), width, height);
        std::ostringstream name; name << std::hex << glyph.code << ".png";
        output.push_back({options.output / name.str(), std::move(png)});
        json record = {{"code", glyph.code}, {"advanceWidth", glyph.advance16 / 16.0}, {"bbox", {{"x", glyph.x}, {"y", glyph.y}, {"width", glyph.width}, {"height", glyph.height}}}, {"kerning", json::object()}};
        if (index < font.debug.size()) {
            const auto& debug = font.debug[index];
            static const char* names[] = {"width", "height", "horiBearingX", "horiBearingY", "horiAdvance", "vertBearingX", "vertBearingY", "vertAdvance"};
            json metrics = json::object(); for (size_t i = 0; i < debug.metrics26.size(); ++i) metrics[names[i]] = debug.metrics26[i] / 64.0;
            record["freetype"] = {{"metrics", metrics}, {"linearHoriAdvance", debug.linearHori16 / 65536.0}, {"linearVertAdvance", debug.linearVert16 / 65536.0}, {"advance", {{"x", debug.advanceX26 / 64.0}, {"y", debug.advanceY26 / 64.0}}}, {"bitmap", {{"width", glyph.width}, {"rows", glyph.height}, {"pitch", debug.pitch}, {"num_grays", debug.numGrays}, {"pixel_mode", debug.pixelMode}, {"palette_mode", debug.paletteMode}}}, {"bitmap_left", glyph.x}, {"bitmap_top", glyph.y + int32_t(glyph.height)}, {"lsb_delta", debug.lsb26 / 64.0}, {"rsb_delta", debug.rsb26 / 64.0}};
        }
        for (const auto& pair : font.kerning) if (pair.left == index + 1) record["kerning"][std::to_string(font.glyphs[pair.right - 1].code)] = pair.value16 / 16.0;
        if (options.fullInfo) {
            record["pixels"] = json::array();
            const auto maximum = (1u << options.bpp) - 1;
            for (unsigned row = 0; row < glyph.height; ++row) {
                std::vector<uint8_t> values(glyph.pixels.begin() + size_t(row) * glyph.width, glyph.pixels.begin() + size_t(row + 1) * glyph.width);
                for (auto& value : values) value = static_cast<uint8_t>(((uint32_t(value >> (8 - options.bpp)) * 65535u) / maximum) >> 8);
                record["pixels"].push_back(values);
            }
        }
        info["glyphs"].push_back(std::move(record));
    }
    auto text = info.dump(2);
    output.push_back({options.output / "font_info.json", std::vector<uint8_t>(text.begin(), text.end())});
    return output;
}
}
