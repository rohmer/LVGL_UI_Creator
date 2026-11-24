#pragma once
#include <ft2build.h>
#include FT_TRUETYPE_TABLES_H
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace FontConversion::Native {
// The OpenType pair positioning used by the previous opentype.js converter.
// Select DFLT/latn's default language and kern feature, rather than applying
// unrelated positioning features or pairs from a different script.
class GposKerning {
    std::vector<uint8_t> bytes;
    std::vector<size_t> tables;
    uint16_t U16(size_t at) const {
        if (at > bytes.size() || bytes.size() - at < 2) throw std::runtime_error("Invalid OpenType GPOS table");
        return uint16_t((uint16_t(bytes[at]) << 8) | bytes[at + 1]);
    }
    uint32_t U32(size_t at) const { return (uint32_t(U16(at)) << 16) | U16(at + 2); }
    static size_t ValueSize(uint16_t format) {
        size_t result = 0;
        for (unsigned bit = 0; bit < 16; ++bit) if (format & (1u << bit)) result += 2;
        return result;
    }
    int XAdvance(size_t at, uint16_t format) const {
        if (!(format & 4)) return 0;
        return static_cast<int16_t>(U16(at + ((format & 1) ? 2 : 0) + ((format & 2) ? 2 : 0)));
    }
    int Coverage(size_t at, uint16_t glyph) const {
        const auto format = U16(at), count = U16(at + 2);
        if (format == 1) {
            size_t low = 0, high = count;
            while (low < high) { auto mid = low + (high - low) / 2; auto value = U16(at + 4 + mid * 2); if (value < glyph) low = mid + 1; else high = mid; }
            return low < count && U16(at + 4 + low * 2) == glyph ? static_cast<int>(low) : -1;
        }
        if (format == 2) {
            size_t low = 0, high = count;
            while (low < high) { auto mid = low + (high - low) / 2; if (U16(at + 4 + mid * 6 + 2) < glyph) low = mid + 1; else high = mid; }
            if (low < count && glyph >= U16(at + 4 + low * 6)) return U16(at + 8 + low * 6) + glyph - U16(at + 4 + low * 6);
        }
        return -1;
    }
    uint16_t Class(size_t at, uint16_t glyph) const {
        auto format = U16(at);
        if (format == 1) {
            const auto first = U16(at + 2), count = U16(at + 4);
            return glyph >= first && uint32_t(glyph) - first < count ? U16(at + 6 + (glyph - first) * 2) : 0;
        }
        if (format == 2) {
            auto count = U16(at + 2); size_t low = 0, high = count;
            while (low < high) { auto mid = low + (high - low) / 2; if (U16(at + 4 + mid * 6 + 2) < glyph) low = mid + 1; else high = mid; }
            if (low < count && glyph >= U16(at + 4 + low * 6)) return U16(at + 8 + low * 6);
        }
        return 0;
    }
public:
    explicit GposKerning(FT_Face face) {
        FT_ULong size = 0;
        if (FT_Load_Sfnt_Table(face, FT_MAKE_TAG('G','P','O','S'), 0, nullptr, &size) || !size) return;
        if (size > 64u * 1024u * 1024u) throw std::runtime_error("OpenType GPOS table is too large");
        bytes.resize(size);
        if (FT_Load_Sfnt_Table(face, FT_MAKE_TAG('G','P','O','S'), 0, bytes.data(), &size)) throw std::runtime_error("Cannot read OpenType GPOS table");
        const auto scripts = U16(4), features = U16(6), lookups = U16(8);
        size_t script = 0;
        for (unsigned i = 0; i < U16(scripts); ++i) {
            const auto record = scripts + 2 + i * 6; auto tag = U32(record);
            if (tag == FT_MAKE_TAG('D','F','L','T')) { script = scripts + U16(record + 4); break; }
            if (tag == FT_MAKE_TAG('l','a','t','n')) script = scripts + U16(record + 4);
        }
        if (!script || !U16(script)) return;
        const auto language = script + U16(script);
        for (unsigned i = 0; i < U16(language + 4); ++i) {
            const auto index = U16(language + 6 + i * 2);
            if (index >= U16(features)) throw std::runtime_error("Invalid GPOS feature index");
            const auto record = features + 2 + index * 6;
            if (U32(record) != FT_MAKE_TAG('k','e','r','n')) continue;
            const auto feature = features + U16(record + 4);
            for (unsigned j = 0; j < U16(feature + 2); ++j) {
                const auto lookupIndex = U16(feature + 4 + j * 2);
                if (lookupIndex >= U16(lookups)) throw std::runtime_error("Invalid GPOS lookup index");
                const auto lookup = lookups + U16(lookups + 2 + lookupIndex * 2);
                const auto type = U16(lookup);
                if (type != 2 && type != 9) continue;
                for (unsigned k = 0; k < U16(lookup + 4); ++k) {
                    size_t table = lookup + U16(lookup + 6 + k * 2);
                    if (type == 9) {
                        if (U16(table) != 1 || U16(table + 2) != 2) continue;
                        table += U32(table + 4);
                    }
                    tables.push_back(table);
                }
            }
            break; // Match the first default-language kern feature.
        }
    }
    bool Present() const { return !bytes.empty(); }
    bool HasPairs() const { return !tables.empty(); }
    bool HasLeft(uint16_t glyph) const {
        for (const auto table : tables) if (Coverage(table + U16(table + 2), glyph) >= 0) return true;
        return false;
    }
    int Pair(uint16_t left, uint16_t right) const {
        for (const auto table : tables) {
            const auto coverage = Coverage(table + U16(table + 2), left);
            if (coverage < 0) continue;
            const auto format = U16(table), value1 = U16(table + 4), value2 = U16(table + 6);
            const auto size1 = ValueSize(value1), recordSize = size1 + ValueSize(value2);
            if (format == 1) {
                if (coverage >= U16(table + 8)) throw std::runtime_error("Invalid GPOS pair-set index");
                const auto set = table + U16(table + 10 + coverage * 2);
                size_t low = 0, high = U16(set);
                while (low < high) { const auto mid = low + (high - low) / 2; if (U16(set + 2 + mid * (recordSize + 2)) < right) low = mid + 1; else high = mid; }
                if (low < U16(set) && U16(set + 2 + low * (recordSize + 2)) == right) return XAdvance(set + 4 + low * (recordSize + 2), value1);
            } else if (format == 2) {
                const auto class1 = Class(table + U16(table + 8), left), class2 = Class(table + U16(table + 10), right);
                const auto count1 = U16(table + 12), count2 = U16(table + 14);
                if (class1 >= count1 || class2 >= count2) throw std::runtime_error("Invalid GPOS pair class");
                return XAdvance(table + 16 + (size_t(class1) * count2 + class2) * recordSize, value1);
            }
        }
        return 0;
    }
};
}
