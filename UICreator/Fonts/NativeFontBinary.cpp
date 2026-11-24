// Native implementation of the LVGL font binary/bitmap format.
// The format and modified I3BN codec follow lv_font_conv, Copyright (c) 2018
// authors, MIT licensed. Its complete notice is retained at
// 3rdParty/lv_font_conv/LICENSE and distributed with the converter notices.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#include "NativeFontData.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace FontConversion::Native {
namespace {
using Bytes = std::vector<uint8_t>;
class Bits {
public:
    void Write(uint32_t value, unsigned count) {
        if (count > 32) throw std::runtime_error("Font bit field exceeds 32 bits");
        for (unsigned i = count; i > 0; --i) {
            if ((position_ & 7) == 0) bytes_.push_back(0);
            bytes_.back() |= uint8_t(((value >> (i - 1)) & 1) << (7 - (position_ & 7)));
            ++position_;
        }
    }
    void Zeros(size_t count) {
        while (count > 0) { const auto part = unsigned((std::min)(count, size_t(32))); Write(0, part); count -= part; }
    }
    Bytes Finish(size_t alignment = 1) {
        if (alignment == 0) throw std::runtime_error("Font alignment must be positive");
        if (bytes_.size() % alignment) bytes_.resize(bytes_.size() + alignment - bytes_.size() % alignment, 0);
        return std::move(bytes_);
    }
private:
    Bytes bytes_;
    size_t position_ = 0;
};
size_t Align4(size_t value) {
    if (value > (std::numeric_limits<size_t>::max)() - 3) throw std::runtime_error("Font table is too large");
    return (value + 3) & ~size_t(3);
}
uint32_t U32(size_t value) {
    if (value > (std::numeric_limits<uint32_t>::max)()) throw std::runtime_error("Font table exceeds 4 GiB");
    return uint32_t(value);
}
uint16_t U16(int64_t value) {
    if (value < 0 || value > 65535) throw std::runtime_error("Font metric exceeds its unsigned 16-bit field");
    return uint16_t(value);
}
uint16_t S16(int64_t value) {
    if (value < -32768 || value > 32767) throw std::runtime_error("Font metric exceeds its signed 16-bit field");
    return uint16_t(value);
}
void Put16(Bytes& bytes, size_t at, uint16_t value) {
    bytes.at(at) = uint8_t(value); bytes.at(at + 1) = uint8_t(value >> 8);
}
void Put32(Bytes& bytes, size_t at, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.at(at + i) = uint8_t(value >> (i * 8));
}
void Add16(Bytes& bytes, uint16_t value) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8)); }
void Add32(Bytes& bytes, uint32_t value) { for (unsigned i = 0; i < 4; ++i) bytes.push_back(uint8_t(value >> (i * 8))); }
void Append(Bytes& bytes, const Bytes& other) {
    if (other.size() > (std::numeric_limits<uint32_t>::max)() - bytes.size()) throw std::runtime_error("Font exceeds 4 GiB");
    bytes.insert(bytes.end(), other.begin(), other.end());
}
void CompleteTable(Bytes& table, const char* name) {
    table.resize(Align4(table.size()), 0);
    Put32(table, 0, U32(table.size()));
    for (unsigned i = 0; i < 4; ++i) table.at(4 + i) = uint8_t(name[i]);
}
unsigned UnsignedBits(uint32_t value) { unsigned bits = 0; while (value) { ++bits; value >>= 1; } return bits; }
unsigned SignedBits(int32_t value) { return UnsignedBits(value < 0 ? uint32_t(-(int64_t(value) + 1)) : uint32_t(value)) + 1; }
int32_t RoundRatio(int64_t numerator, int64_t denominator) {
    // Match Math.round used by the reference converter, including negative ties.
    return int32_t(std::floor(double(numerator) / double(denominator) + 0.5));
}
void ValidateOptions(const Options& options) {
    if (options.bpp != 1 && options.bpp != 2 && options.bpp != 3 && options.bpp != 4 && options.bpp != 8)
        throw std::runtime_error("Font bit depth must be 1, 2, 3, 4 or 8");
    if (options.stride < 0 || options.align <= 0) throw std::runtime_error("Invalid font stride or alignment");
    if (options.smoothing < 0 || options.smoothing > 2) throw std::runtime_error("Invalid font subpixel mode");
}
void Pixels(Bits& bits, const Glyph& glyph, const Options& options) {
    const uint64_t count = uint64_t(glyph.width) * glyph.height;
    if (count != glyph.pixels.size()) throw std::runtime_error("Font glyph bitmap dimensions do not match its pixels");
    const unsigned shift = unsigned(8 - options.bpp);
    const int compression = Compression(options);
    if (!compression) {
        const size_t rowBits = size_t(glyph.width) * unsigned(options.bpp);
        size_t rowPadding = 0;
        if (options.stride > 0) {
            const size_t rowBytes = (rowBits + 7) / 8;
            const size_t stride = size_t(options.stride);
            const size_t aligned = ((rowBytes + stride - 1) / stride) * stride;
            rowPadding = aligned * 8 - rowBits;
        }
        size_t at = 0;
        for (uint32_t y = 0; y < glyph.height; ++y) {
            for (uint32_t x = 0; x < glyph.width; ++x) bits.Write(glyph.pixels[at++] >> shift, unsigned(options.bpp));
            bits.Zeros(rowPadding);
        }
        return;
    }
    auto pixel = [&](size_t at) -> uint8_t {
        auto value = uint8_t(glyph.pixels[at] >> shift);
        if (compression == 1 && at >= glyph.width) value ^= uint8_t(glyph.pixels[at - glyph.width] >> shift);
        return value;
    };
    for (size_t at = 0; at < glyph.pixels.size();) {
        const uint8_t value = pixel(at);
        size_t same = 1;
        // The 6-bit counter represents at most 75 equal pixels per run.
        while (same < 75 && at + same < glyph.pixels.size() && pixel(at + same) == value) ++same;
        at += same;
        bits.Write(value, unsigned(options.bpp));
        if (same == 1) continue;
        --same;
        bits.Write(value, unsigned(options.bpp));
        if (same <= 10) {
            for (size_t i = 0; i < same; ++i) bits.Write(i + 1 < same ? 1 : 0, 1);
        } else {
            for (unsigned i = 0; i < 11; ++i) bits.Write(1, 1);
            bits.Write(uint32_t(same - 11), 6);
        }
    }
}
std::vector<size_t> PairOrder(const FontData& font) {
    std::vector<size_t> order(font.kerning.size()); std::iota(order.begin(), order.end(), size_t(0));
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const auto& x = font.kerning[a]; const auto& y = font.kerning[b];
        return std::make_pair(x.left, x.right) < std::make_pair(y.left, y.right);
    });
    return order;
}
Bytes Cmap(const FontData& font) {
    struct Range { size_t begin, end; bool contiguous; };
    std::vector<Range> ranges;
    for (size_t begin = 0; begin < font.glyphs.size();) {
        size_t end = begin + 1;
        while (end < font.glyphs.size() && font.glyphs[end].code == font.glyphs[end - 1].code + 1 &&
               font.glyphs[end].code - font.glyphs[begin].code < 65535) ++end;
        const bool longRun = end - begin >= 8;
        // Split out long consecutive blocks; sparse tails use two bytes per
        // glyph. This linear planner avoids quadratic Unicode-range searches.
        while (!longRun && end < font.glyphs.size() && font.glyphs[end].code - font.glyphs[begin].code < 65535) {
            size_t run = end + 1;
            while (run < font.glyphs.size() && font.glyphs[run].code == font.glyphs[run - 1].code + 1 &&
                   font.glyphs[run].code - font.glyphs[begin].code < 65535) ++run;
            if (run - end >= 8) break;
            end = run;
        }
        ranges.push_back({begin, end, font.glyphs[end - 1].code - font.glyphs[begin].code + 1 == end - begin});
        begin = end;
    }
    Bytes result(12 + 16 * ranges.size(), 0);
    Put32(result, 8, U32(ranges.size()));
    for (size_t i = 0; i < ranges.size(); ++i) {
        const auto& range = ranges[i]; const size_t header = 12 + i * 16;
        const uint32_t first = font.glyphs[range.begin].code, last = font.glyphs[range.end - 1].code;
        Put32(result, header, U32(result.size())); Put32(result, header + 4, first);
        Put16(result, header + 8, U16(uint64_t(last) - first + 1));
        Put16(result, header + 10, U16(range.begin + 1));
        Put16(result, header + 12, U16(range.contiguous ? uint64_t(last) - first + 1 : range.end - range.begin));
        result[header + 14] = range.contiguous ? 2 : 3;
        if (!range.contiguous) {
            for (size_t j = range.begin; j < range.end; ++j) Add16(result, U16(font.glyphs[j].code - first));
            result.resize(Align4(result.size()), 0);
        }
    }
    CompleteTable(result, "cmap");
    return result;
}
}

int Compression(const Options& options) {
    // LVGL's compressed font decoder supports 2/3/4 bpp. Keep 1/8-bpp
    // glyphs raw so a checked compression option never creates unreadable data.
    if (!options.compress || options.bpp == 1 || options.bpp == 8) return 0;
    return options.prefilter ? 1 : 2;
}

std::vector<uint8_t> Bitmap(const Glyph& glyph, const Options& options) {
    ValidateOptions(options);
    Bits bits; Pixels(bits, glyph, options); return bits.Finish(size_t(options.align));
}

KernData Kerning(const FontData& font, const Options& options) {
    KernData result;
    if (!options.kerning || font.kerning.empty()) return result;
    const size_t count = font.glyphs.size() + 1;
    int64_t maximum = 0;
    for (const auto& pair : font.kerning) {
        if (pair.left == 0 || pair.right == 0 || pair.left >= count || pair.right >= count)
            throw std::runtime_error("Font kerning pair refers to a missing glyph");
        maximum = (std::max)(maximum, pair.value16 < 0 ? -int64_t(pair.value16) : int64_t(pair.value16));
    }
    if (maximum >= 120) result.scale16 = U16((maximum * 2 + 14) / 15);
    result.values.reserve(font.kerning.size());
    for (const auto& pair : font.kerning) {
        const int32_t value = RoundRatio(int64_t(pair.value16) * 16, result.scale16);
        if (value < -128 || value > 127) throw std::runtime_error("Font kerning value exceeds signed 8-bit storage");
        result.values.push_back(int8_t(value));
    }
    if (count > 65535) return result;
    using Row = std::vector<std::pair<uint16_t, int32_t>>;
    std::vector<Row> left(count), right(count);
    for (const auto& pair : font.kerning) {
        left[pair.left].push_back({pair.right, pair.value16}); right[pair.right].push_back({pair.left, pair.value16});
    }
    auto classes = [&](std::vector<Row>& rows, std::vector<uint8_t>& mapping, std::vector<uint16_t>& representatives) {
        std::map<Row, uint8_t> lookup; mapping.resize(count, 0);
        for (size_t id = 1; id < count; ++id) {
            if (rows[id].empty()) continue;
            std::sort(rows[id].begin(), rows[id].end());
            auto found = lookup.find(rows[id]);
            if (found == lookup.end()) {
                if (representatives.size() >= 254) return false;
                representatives.push_back(uint16_t(id));
                found = lookup.emplace(rows[id], uint8_t(representatives.size())).first;
            }
            mapping[id] = found->second;
        }
        return true;
    };
    std::vector<uint8_t> leftMap, rightMap;
    std::vector<uint16_t> leftRepresentatives, rightRepresentatives;
    if (!classes(left, leftMap, leftRepresentatives) || !classes(right, rightMap, rightRepresentatives)) return result;
    const size_t pairSize = Align4(4 + font.kerning.size() * (count > 256 ? 5 : 3));
    const size_t classSize = Align4(4 + 2 * count + leftRepresentatives.size() * rightRepresentatives.size());
    if (!options.fastKerning && classSize > pairSize) return result;
    result.classes = true; result.leftMapping = std::move(leftMap); result.rightMapping = std::move(rightMap);
    result.leftClasses = uint8_t(leftRepresentatives.size()); result.rightClasses = uint8_t(rightRepresentatives.size());
    result.values.clear(); result.values.reserve(size_t(result.leftClasses) * result.rightClasses);
    for (auto leftId : leftRepresentatives) {
        for (auto rightId : rightRepresentatives) {
            const auto& row = left[leftId];
            auto found = std::lower_bound(row.begin(), row.end(), rightId,
                [](const auto& entry, uint16_t id) { return entry.first < id; });
            int32_t value = found != row.end() && found->first == rightId ? found->second : 0;
            result.values.push_back(int8_t(RoundRatio(int64_t(value) * 16, result.scale16)));
        }
    }
    return result;
}

std::vector<uint8_t> Binary(const FontData& font, const Options& options) {
    ValidateOptions(options);
    if (font.glyphs.empty() || font.glyphs.size() > 65535) throw std::runtime_error("Font must contain between 1 and 65535 glyphs");
    for (size_t i = 0; i < font.glyphs.size(); ++i) {
        if (i && font.glyphs[i].code <= font.glyphs[i - 1].code) throw std::runtime_error("Font code points must be sorted and unique");
    }
    const auto kern = Kerning(font, options);
    const bool hasKern = !kern.values.empty();
    bool monospaced = true;
    unsigned xyBits = 1, whBits = 0, advanceBits = 1;
    int32_t minY = (std::numeric_limits<int32_t>::max)(), maxY = (std::numeric_limits<int32_t>::min)();
    auto advance = [&](const Glyph& glyph) { return hasKern ? glyph.advance16 : RoundRatio(glyph.advance16, 16); };
    for (const auto& glyph : font.glyphs) {
        monospaced = monospaced && glyph.advance16 == font.glyphs.front().advance16;
        xyBits = (std::max)({xyBits, SignedBits(glyph.x), SignedBits(glyph.y)});
        whBits = (std::max)({whBits, UnsignedBits(glyph.width), UnsignedBits(glyph.height)});
        advanceBits = (std::max)(advanceBits, SignedBits(advance(glyph)));
        minY = (std::min)(minY, glyph.y);
        if (int64_t(glyph.y) + glyph.height > (std::numeric_limits<int32_t>::max)()) throw std::runtime_error("Font glyph extent is too large");
        maxY = (std::max)(maxY, int32_t(int64_t(glyph.y) + glyph.height));
    }
    Bytes glyf(8, 0);
    std::vector<uint32_t> offsets; offsets.reserve(font.glyphs.size() + 1); offsets.push_back(8);
    for (const auto& glyph : font.glyphs) {
        offsets.push_back(U32(glyf.size())); Bits bits;
        if (!monospaced) bits.Write(uint32_t(advance(glyph)), advanceBits);
        bits.Write(uint32_t(glyph.x), xyBits); bits.Write(uint32_t(glyph.y), xyBits);
        bits.Write(glyph.width, whBits); bits.Write(glyph.height, whBits);
        Pixels(bits, glyph, options); Append(glyf, bits.Finish(size_t(options.align)));
    }
    CompleteTable(glyf, "glyf");
    const bool longOffsets = glyf.size() > 65535;
    const bool longIds = font.glyphs.size() > 255;
    Bytes head(48, 0); Put32(head, 8, 1); Put16(head, 12, hasKern ? 4 : 3);
    Put16(head, 14, U16(font.size)); Put16(head, 16, U16(font.ascent)); Put16(head, 18, S16(font.descent));
    Put16(head, 20, U16(font.typoAscent)); Put16(head, 22, S16(font.typoDescent)); Put16(head, 24, U16(font.typoLineGap));
    Put16(head, 26, S16(minY)); Put16(head, 28, S16(maxY));
    Put16(head, 30, monospaced ? U16(advance(font.glyphs.front())) : 0); Put16(head, 32, kern.scale16);
    head[34] = longOffsets; head[35] = longIds; head[36] = hasKern;
    head[37] = uint8_t(options.bpp); head[38] = uint8_t(xyBits); head[39] = uint8_t(whBits);
    head[40] = monospaced ? 0 : uint8_t(advanceBits); head[41] = uint8_t(Compression(options));
    head[42] = uint8_t(options.smoothing);
    Put16(head, 44, S16(font.underlinePosition)); Put16(head, 46, U16(font.underlineThickness));
    CompleteTable(head, "head");
    Bytes loca(12, 0); Put32(loca, 8, U32(offsets.size()));
    for (auto offset : offsets) { if (longOffsets) Add32(loca, offset); else Add16(loca, U16(offset)); }
    CompleteTable(loca, "loca");
    Bytes result = std::move(head); Append(result, Cmap(font)); Append(result, loca); Append(result, glyf);
    if (hasKern) {
        Bytes table(12, 0); table[8] = kern.classes ? 3 : 0;
        if (kern.classes) {
            Add16(table, U16(font.glyphs.size() + 1)); table.push_back(kern.leftClasses); table.push_back(kern.rightClasses);
            Append(table, kern.leftMapping); Append(table, kern.rightMapping);
            for (auto value : kern.values) table.push_back(uint8_t(value));
        } else {
            Add32(table, U32(font.kerning.size())); const auto order = PairOrder(font);
            for (auto index : order) {
                const auto& pair = font.kerning[index];
                if (longIds) { Add16(table, pair.left); Add16(table, pair.right); }
                else { table.push_back(uint8_t(pair.left)); table.push_back(uint8_t(pair.right)); }
            }
            for (auto index : order) table.push_back(uint8_t(kern.values[index]));
        }
        CompleteTable(table, "kern"); Append(result, table);
    }
    return result;
}
}
