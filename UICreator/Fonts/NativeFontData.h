#pragma once
#include "FontConverter.h"
#include <cstdint>
#include <array>
#include <vector>

namespace FontConversion::Native {
struct Glyph {
    uint32_t code = 0;
    int32_t advance16 = 0;
    int32_t x = 0, y = 0;
    uint32_t width = 0, height = 0;
    // Top-down, tightly packed 8-bit opacity (before output quantization).
    std::vector<uint8_t> pixels;
};
struct KernPair {
    uint16_t left = 0, right = 0; // Sorted one-based glyph IDs; ID zero is reserved.
    int32_t value16 = 0;
};
struct GlyphDebug {
    std::array<int32_t, 8> metrics26{};
    int32_t linearHori16 = 0, linearVert16 = 0, advanceX26 = 0, advanceY26 = 0;
    int32_t lsb26 = 0, rsb26 = 0, pitch = 0;
    uint16_t numGrays = 0;
    uint8_t pixelMode = 0, paletteMode = 0;
};
struct FontData {
    int size = 0, ascent = 0, descent = 0;
    int typoAscent = 0, typoDescent = 0, typoLineGap = 0;
    int capHeight = 0, xHeight = 0, underlinePosition = 0, underlineThickness = 1;
    // Glyphs are sorted by Unicode code point, with no ID-zero entry.
    std::vector<Glyph> glyphs;
    std::vector<KernPair> kerning;
    // Collected only for debug dumps, avoiding per-glyph diagnostic storage
    // during normal C/binary conversion.
    std::vector<GlyphDebug> debug;
};
struct KernData {
    uint16_t scale16 = 16;
    bool classes = false;
    std::vector<uint8_t> leftMapping, rightMapping;
    uint8_t leftClasses = 0, rightClasses = 0;
    std::vector<int8_t> values;
};
// Portable LVGL bitmap codec and binary font writer. Neither depends on LVGL
// host configuration or on an external converter process.
int Compression(const Options& options);
std::vector<uint8_t> Bitmap(const Glyph& glyph, const Options& options);
KernData Kerning(const FontData& font, const Options& options);
std::vector<uint8_t> Binary(const FontData& font, const Options& options);
}
