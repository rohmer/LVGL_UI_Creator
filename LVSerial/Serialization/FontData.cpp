#include "FontData.h"
#include "Base64.h"
#include <stdexcept>
#include <limits>

namespace Serialization {
namespace {
using json = nlohmann::json;
bool CanonicalBase64(const std::string& value) {
    if(value.size()%4)return false;
    const auto digit=[](unsigned char ch)->int {
        if(ch>='A'&&ch<='Z')return ch-'A';
        if(ch>='a'&&ch<='z')return ch-'a'+26;
        if(ch>='0'&&ch<='9')return ch-'0'+52;
        if(ch=='+')return 62;
        if(ch=='/')return 63;
        return -1;
    };
    size_t padding=0;
    if(!value.empty()&&value.back()=='='){padding=1;if(value.size()>1&&value[value.size()-2]=='=')padding=2;}
    for(size_t i=0;i<value.size()-padding;++i)if(digit(static_cast<unsigned char>(value[i]))<0)return false;
    if(padding==1&&(digit(static_cast<unsigned char>(value[value.size()-2]))&3))return false;
    if(padding==2&&(digit(static_cast<unsigned char>(value[value.size()-3]))&15))return false;
    return true;
}
bool Keys(const json& value, std::initializer_list<const char*> allowed) {
    if (!value.is_object()) return false;
    for (const auto& item : value.items()) {
        bool found = false;
        for (const auto* key : allowed) if (item.key() == key) { found = true; break; }
        if (!found) return false;
    }
    return true;
}
uint32_t U32(const json& value) {
    if (!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() && value.get<int64_t>() < 0) ||
        value.get<uint64_t>() > UINT32_MAX) throw std::invalid_argument("Invalid unsigned font metric");
    return value.get<uint32_t>();
}
int32_t I32(const json& value) {
    if (!value.is_number_integer() || (value.is_number_unsigned() && value.get<uint64_t>() > INT32_MAX) ||
        (!value.is_number_unsigned() && (value.get<int64_t>() < INT32_MIN || value.get<int64_t>() > INT32_MAX)))
        throw std::invalid_argument("Invalid signed font metric");
    return value.get<int32_t>();
}
struct Bytes { std::string value; bool present = false; };
bool WriteBytes(pb_ostream_t* stream, const pb_field_t* field, void* const* argument) {
    const auto& bytes = *static_cast<const std::string*>(*argument);
    return pb_encode_tag_for_field(stream, field) &&
        pb_encode_string(stream, reinterpret_cast<const pb_byte_t*>(bytes.data()), bytes.size());
}
bool ReadBytes(pb_istream_t* stream, const pb_field_t*, void** argument) {
    auto& bytes = *static_cast<Bytes*>(*argument);
    if (bytes.present) PB_RETURN_ERROR(stream, "duplicate font byte table");
    bytes.present = true;
    bytes.value.resize(stream->bytes_left);
    return pb_read(stream, reinterpret_cast<pb_byte_t*>(bytes.value.data()), bytes.value.size());
}
void Bind(pb_callback_t& callback, const std::string& bytes) {
    callback.funcs.encode = WriteBytes; callback.arg = const_cast<std::string*>(&bytes);
}
void Bind(pb_callback_t& callback, Bytes& bytes) { callback.funcs.decode = ReadBytes; callback.arg = &bytes; }
bool WriteNumbers(pb_ostream_t* stream, const pb_field_t* field, void* const* argument) {
    const auto& numbers = *static_cast<const json*>(*argument);
    if (numbers.empty()) return true;
    pb_ostream_t size = PB_OSTREAM_SIZING;
    for (const auto& number : numbers) if (!pb_encode_varint(&size, U32(number))) return false;
    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag) || !pb_encode_varint(stream, size.bytes_written)) return false;
    for (const auto& number : numbers) if (!pb_encode_varint(stream, U32(number))) return false;
    return true;
}
bool ReadNumbers(pb_istream_t* stream, const pb_field_t*, void** argument) {
    auto& numbers = *static_cast<json*>(*argument);
    while (stream->bytes_left) {
        uint64_t number = 0;
        if (!pb_decode_varint(stream, &number)) return false;
        if (number > UINT32_MAX) PB_RETURN_ERROR(stream, "font table number overflows uint32");
        numbers.push_back(number);
    }
    return true;
}
void Bind(pb_callback_t& callback, const json& numbers) {
    callback.funcs.encode = WriteNumbers; callback.arg = const_cast<json*>(&numbers);
}
void BindRead(pb_callback_t& callback, json& numbers) { callback.funcs.decode = ReadNumbers; callback.arg = &numbers; }

#define FONT_OPTIONAL_FIELDS(X) \
    X(base_line, "baseLine", I32) X(cap_height, "capHeight", U32) X(x_height, "xHeight", U32) \
    X(subpixel_mode, "subpx", U32) X(kerning_mode, "kerning", U32) \
    X(underline_position, "underlinePosition", I32) X(underline_thickness, "underlineThickness", U32)
#define DSC_OPTIONAL_FIELDS(X) \
    X(stride, "stride", U32) X(kern_scale, "kernScale", U32) X(kern_classes, "kernClasses", U32) X(cmap_count, "cmapNum", U32)
#define KERN_OPTIONAL_FIELDS(X) \
    X(glyph_id_size, "glyphIDSize", U32) X(left_count, "leftCount", U32) X(right_count, "rightCount", U32)

bool WriteGlyphs(pb_ostream_t* stream, const pb_field_t* field, void* const* argument) {
    for (const auto& value : *static_cast<const json*>(*argument)) {
        lvserial_FontGlyph glyph = lvserial_FontGlyph_init_zero;
        glyph.bitmap_index = U32(value.at("bitmapIdx")); glyph.advance = U32(value.at("adv_w"));
        glyph.width = U32(value.at("box_w")); glyph.height = U32(value.at("box_h"));
        glyph.offset_x = I32(value.at("ofs_x")); glyph.offset_y = I32(value.at("ofs_y"));
        if (!pb_encode_tag_for_field(stream, field) || !pb_encode_submessage(stream, lvserial_FontGlyph_fields, &glyph)) return false;
    }
    return true;
}
bool ReadGlyphs(pb_istream_t* stream, const pb_field_t*, void** argument) {
    if(static_cast<json*>(*argument)->size()>=65536)PB_RETURN_ERROR(stream,"too many font glyphs");
    lvserial_FontGlyph glyph = lvserial_FontGlyph_init_zero;
    if (!pb_decode(stream, lvserial_FontGlyph_fields, &glyph)) return false;
    static_cast<json*>(*argument)->push_back({{"bitmapIdx",glyph.bitmap_index},{"adv_w",glyph.advance},
        {"box_w",glyph.width},{"box_h",glyph.height},{"ofs_x",glyph.offset_x},{"ofs_y",glyph.offset_y}});
    return true;
}
bool WriteCmaps(pb_ostream_t* stream, const pb_field_t* field, void* const* argument) {
    for (const auto& value : *static_cast<const json*>(*argument)) {
        lvserial_FontCmap map = lvserial_FontCmap_init_zero;
        map.range_start = U32(value.at("rangeStart")); map.range_length = U32(value.at("rangeLength"));
        map.glyph_start = U32(value.at("glyphIDstart")); map.type = U32(value.at("type"));
        if (value.contains("formatVersion")) { map.has_format_version = true; map.format_version = U32(value.at("formatVersion")); }
        Bind(map.unicode_list, value.at("unicodeList")); Bind(map.glyph_offsets, value.at("glyphIDOffsetList"));
        if (!pb_encode_tag_for_field(stream, field) || !pb_encode_submessage(stream, lvserial_FontCmap_fields, &map)) return false;
    }
    return true;
}
bool ReadCmaps(pb_istream_t* stream, const pb_field_t*, void** argument) {
    if(static_cast<json*>(*argument)->size()>=511)PB_RETURN_ERROR(stream,"too many font cmaps");
    lvserial_FontCmap map = lvserial_FontCmap_init_zero;
    json unicode = json::array(), offsets = json::array();
    BindRead(map.unicode_list, unicode); BindRead(map.glyph_offsets, offsets);
    if (!pb_decode(stream, lvserial_FontCmap_fields, &map)) return false;
    if(unicode.size()>65535 || offsets.size()>65535)PB_RETURN_ERROR(stream,"font cmap table exceeds uint16 size");
    json value = {{"rangeStart",map.range_start},{"rangeLength",map.range_length},{"glyphIDstart",map.glyph_start},
        {"type",map.type},{"unicodeList",std::move(unicode)},{"glyphIDOffsetList",std::move(offsets)}};
    if (map.has_format_version) value["formatVersion"] = map.format_version;
    static_cast<json*>(*argument)->push_back(std::move(value));
    return true;
}
std::string ByteArray(const json& values, bool signedValues) {
    std::string result;
    result.reserve(values.size());
    for (const auto& value : values) {
        const auto number = I32(value);
        if (number < (signedValues ? -128 : 0) || number > (signedValues ? 127 : 255))
            throw std::invalid_argument("Font byte table value is out of range");
        result += static_cast<char>(number);
    }
    return result;
}
json Numbers(const std::string& bytes, bool signedValues) {
    json result = json::array();
    for (const auto value : bytes) result.push_back(signedValues ? int(static_cast<int8_t>(value)) : int(static_cast<uint8_t>(value)));
    return result;
}
bool WriteKern(pb_ostream_t* stream, const pb_field_t* field, void* const* argument) {
    const auto& value = *static_cast<const json*>(*argument);
    lvserial_FontKerning kern = lvserial_FontKerning_init_zero;
#define WRITE_OPTIONAL(member, key, convert) if (value.contains(key)) { kern.has_##member = true; kern.member = convert(value.at(key)); }
    KERN_OPTIONAL_FIELDS(WRITE_OPTIONAL)
#undef WRITE_OPTIONAL
    if (value.contains("GlyphIDs")) Bind(kern.glyph_ids, value.at("GlyphIDs"));
    const auto values = ByteArray(value.at("Values"), true);
    Bind(kern.values, values);
    std::string left, right;
    if (value.contains("leftClasses")) { left = ByteArray(value.at("leftClasses"), false); Bind(kern.left_classes, left); }
    if (value.contains("rightClasses")) { right = ByteArray(value.at("rightClasses"), false); Bind(kern.right_classes, right); }
    return pb_encode_tag_for_field(stream, field) && pb_encode_submessage(stream, lvserial_FontKerning_fields, &kern);
}
struct KernRead { json value; bool present = false; };
bool ReadKern(pb_istream_t* stream, const pb_field_t*, void** argument) {
    auto& result = *static_cast<KernRead*>(*argument);
    if (result.present) PB_RETURN_ERROR(stream, "duplicate font kerning table");
    result.present = true;
    lvserial_FontKerning kern = lvserial_FontKerning_init_zero;
    Bytes values, left, right; json ids = json::array();
    Bind(kern.values, values); Bind(kern.left_classes, left); Bind(kern.right_classes, right); BindRead(kern.glyph_ids, ids);
    if (!pb_decode(stream, lvserial_FontKerning_fields, &kern)) return false;
    if (!values.present) PB_RETURN_ERROR(stream, "missing font kerning values");
    result.value = {{"Values",Numbers(values.value, true)}};
#define READ_OPTIONAL(member, key, convert) if (kern.has_##member) result.value[key] = kern.member;
    KERN_OPTIONAL_FIELDS(READ_OPTIONAL)
#undef READ_OPTIONAL
    if (kern.has_glyph_id_size || !ids.empty()) result.value["GlyphIDs"] = std::move(ids);
    if (left.present) result.value["leftClasses"] = Numbers(left.value, false);
    if (right.present) result.value["rightClasses"] = Numbers(right.value, false);
    return true;
}
}

bool FontData::Matches(const json& font) {
    if (!Keys(font,{"baseLine","lineHeight","capHeight","xHeight","subpx","kerning","underlinePosition","underlineThickness","dsc"}) ||
        !font.contains("lineHeight") || !font.contains("dsc")) return false;
    const auto& d = font.at("dsc");
    if (!Keys(d,{"bitmapFmt","bpp","stride","cmapNum","kernScale","kernClasses","cmaps","GlyphDsc","bitmap","Kern"}) ||
        !d.contains("bitmap") || !d["bitmap"].is_string() || !d.contains("bitmapFmt") || !d.contains("bpp") ||
        !d.contains("GlyphDsc") || !d["GlyphDsc"].is_array() || d["GlyphDsc"].size()>65536 ||
        !d.contains("cmaps") || !d["cmaps"].is_array() || d["cmaps"].size()>511) return false;
    for (const auto& glyph : d["GlyphDsc"]) if (!Keys(glyph,{"bitmapIdx","adv_w","box_w","box_h","ofs_x","ofs_y"}) || glyph.size() != 6) return false;
    for (const auto& map : d["cmaps"]) if (!Keys(map,{"rangeStart","rangeLength","glyphIDstart","type","formatVersion","unicodeList","glyphIDOffsetList"}) ||
        !map.contains("rangeStart") || !map.contains("rangeLength") || !map.contains("glyphIDstart") || !map.contains("type") ||
        !map.contains("unicodeList") || !map["unicodeList"].is_array() || !map.contains("glyphIDOffsetList") || !map["glyphIDOffsetList"].is_array()) return false;
    if (d.contains("Kern") && !d["Kern"].is_null()) {
        const auto& kern = d["Kern"];
        if (!Keys(kern,{"Values","GlyphIDs","glyphIDSize","leftCount","rightCount","leftClasses","rightClasses"}) ||
            !kern.contains("Values") || !kern["Values"].is_array() || kern.contains("GlyphIDs")!=kern.contains("glyphIDSize")) return false;
        // The presence of each kind's complete table is maintained on decode.
        const bool pairs = kern.contains("glyphIDSize") && kern.contains("GlyphIDs") && kern["GlyphIDs"].is_array();
        const bool classes = kern.contains("leftCount") && kern.contains("rightCount") && kern.contains("leftClasses") &&
            kern["leftClasses"].is_array() && kern.contains("rightClasses") && kern["rightClasses"].is_array();
        if (!pairs && !classes) return false;
    }
    // Arbitrary JSON records (including malformed metrics used by validators)
    // must remain lossless. Use generic properties when a scalar cannot be
    // represented by this typed schema instead of narrowing or rejecting it.
    try {
        U32(font.at("lineHeight"));U32(d.at("bitmapFmt"));U32(d.at("bpp"));
#define CHECK_OPTIONAL(member, key, convert) if(font.contains(key))convert(font.at(key));
        FONT_OPTIONAL_FIELDS(CHECK_OPTIONAL)
#undef CHECK_OPTIONAL
#define CHECK_OPTIONAL(member, key, convert) if(d.contains(key))convert(d.at(key));
        DSC_OPTIONAL_FIELDS(CHECK_OPTIONAL)
#undef CHECK_OPTIONAL
        const auto& bitmap=d.at("bitmap").get_ref<const std::string&>();
        if(bitmap.size()/4*3>UINT32_MAX || !CanonicalBase64(bitmap))return false;
        for(const auto& glyph:d.at("GlyphDsc")) {
            for(const auto* key:{"bitmapIdx","adv_w","box_w","box_h"})U32(glyph.at(key));
            I32(glyph.at("ofs_x"));I32(glyph.at("ofs_y"));
        }
        for(const auto& map:d.at("cmaps")) {
            for(const auto* key:{"rangeStart","rangeLength","glyphIDstart","type"})U32(map.at(key));
            if(map.contains("formatVersion"))U32(map.at("formatVersion"));
            for(const auto* key:{"unicodeList","glyphIDOffsetList"})for(const auto& value:map.at(key))U32(value);
        }
        if(d.contains("Kern")&&d["Kern"].is_object()) {
            const auto& kern=d["Kern"];
#define CHECK_OPTIONAL(member, key, convert) if(kern.contains(key))convert(kern.at(key));
            KERN_OPTIONAL_FIELDS(CHECK_OPTIONAL)
#undef CHECK_OPTIONAL
            ByteArray(kern.at("Values"),true);
            for(const auto* key:{"leftClasses","rightClasses"})if(kern.contains(key))ByteArray(kern.at(key),false);
            if(kern.contains("GlyphIDs"))for(const auto& value:kern.at("GlyphIDs"))U32(value);
        }
    }catch(const std::exception&){return false;}
    return true;
}

std::vector<uint8_t> FontData::Encode(const json& font) {
    if (!Matches(font)) throw std::invalid_argument("Unsupported portable font data");
    const auto& d = font.at("dsc");
    lvserial_FontData message = lvserial_FontData_init_zero;
    message.format_version = 1; message.line_height = U32(font.at("lineHeight"));
#define WRITE_OPTIONAL(member, key, convert) if (font.contains(key)) { message.has_##member = true; message.member = convert(font.at(key)); }
    FONT_OPTIONAL_FIELDS(WRITE_OPTIONAL)
#undef WRITE_OPTIONAL
#define WRITE_OPTIONAL(member, key, convert) if (d.contains(key)) { message.has_##member = true; message.member = convert(d.at(key)); }
    DSC_OPTIONAL_FIELDS(WRITE_OPTIONAL)
#undef WRITE_OPTIONAL
    const auto bitmap = base64_decode(d.at("bitmap").get<std::string>());
    Bind(message.bitmap, bitmap);
    message.bitmap_format = U32(d.at("bitmapFmt")); message.bpp = U32(d.at("bpp"));
    message.glyphs.funcs.encode = WriteGlyphs; message.glyphs.arg = const_cast<json*>(&d.at("GlyphDsc"));
    message.cmaps.funcs.encode = WriteCmaps; message.cmaps.arg = const_cast<json*>(&d.at("cmaps"));
    if (d.contains("Kern")) {
        if (d["Kern"].is_null()) { message.has_kern_is_null = true; message.kern_is_null = true; }
        else { message.kern.funcs.encode = WriteKern; message.kern.arg = const_cast<json*>(&d.at("Kern")); }
    }
    pb_ostream_t sizing = PB_OSTREAM_SIZING;
    if (!pb_encode(&sizing, lvserial_FontData_fields, &message)) throw std::invalid_argument(PB_GET_ERROR(&sizing));
    std::vector<uint8_t> result(sizing.bytes_written);
    auto output = pb_ostream_from_buffer(result.data(), result.size());
    if (!pb_encode(&output, lvserial_FontData_fields, &message)) throw std::invalid_argument(PB_GET_ERROR(&output));
    return result;
}

json FontData::Decode(pb_istream_t& input) {
    lvserial_FontData message = lvserial_FontData_init_zero;
    Bytes bitmap; KernRead kern;
    json glyphs = json::array(), cmaps = json::array();
    Bind(message.bitmap, bitmap);
    message.glyphs.funcs.decode = ReadGlyphs; message.glyphs.arg = &glyphs;
    message.cmaps.funcs.decode = ReadCmaps; message.cmaps.arg = &cmaps;
    message.kern.funcs.decode = ReadKern; message.kern.arg = &kern;
    if (!pb_decode(&input, lvserial_FontData_fields, &message)) throw std::invalid_argument(PB_GET_ERROR(&input));
    if (message.format_version != 1 || !bitmap.present || (kern.present && message.has_kern_is_null))
        throw std::invalid_argument("Invalid or unsupported font data");
    json result = {{"lineHeight",message.line_height}};
#define READ_OPTIONAL(member, key, convert) if (message.has_##member) result[key] = message.member;
    FONT_OPTIONAL_FIELDS(READ_OPTIONAL)
#undef READ_OPTIONAL
    auto& d = result["dsc"];
    d = {{"bitmapFmt",message.bitmap_format},{"bpp",message.bpp},{"GlyphDsc",std::move(glyphs)},
        {"cmaps",std::move(cmaps)},{"bitmap",base64_encode(reinterpret_cast<const unsigned char*>(bitmap.value.data()), bitmap.value.size())}};
#define READ_OPTIONAL(member, key, convert) if (message.has_##member) d[key] = message.member;
    DSC_OPTIONAL_FIELDS(READ_OPTIONAL)
#undef READ_OPTIONAL
    if (kern.present) d["Kern"] = std::move(kern.value);
    if (message.has_kern_is_null) {
        if (!message.kern_is_null) throw std::invalid_argument("Invalid font kerning null marker");
        d["Kern"] = nullptr;
    }
    return result;
}

json FontData::ToXML(const json& font) {
    auto result = font;
    const auto bytes = base64_decode(result.at("dsc").at("bitmap").get<std::string>());
    result["dsc"]["bitmap"] = json::binary(std::vector<uint8_t>(bytes.begin(), bytes.end()));
    return result;
}
json FontData::FromXML(json font) {
    if (!font.is_object() || !font.contains("dsc") || !font["dsc"].is_object() ||
        !font["dsc"].contains("bitmap") || !font["dsc"]["bitmap"].is_binary())
        throw std::invalid_argument("Font data requires a binary bitmap");
    const auto& bytes = font["dsc"]["bitmap"].get_binary();
    font["dsc"]["bitmap"] = base64_encode(bytes.data(), bytes.size());
    if (!Matches(font)) throw std::invalid_argument("Invalid font data");
    return font;
}
}
