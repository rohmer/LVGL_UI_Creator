#include "Xml.h"
#include "FontData.h"
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace Serialization
{
namespace
{
using json = nlohmann::json;
using View = std::string_view;
constexpr size_t hardDepthLimit = 256;

[[noreturn]] void Fail(const std::string& reason)
{
    throw std::invalid_argument("UI Creator XML: " + reason);
}
bool Space(char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }
bool XmlCharacter(uint32_t value)
{
    return value == 9 || value == 10 || value == 13 ||
        (value >= 0x20 && value <= 0xd7ff) ||
        (value >= 0xe000 && value <= 0xfffd) || (value >= 0x10000 && value <= 0x10ffff);
}
void ValidateText(View value)
{
    for (size_t offset = 0; offset < value.size();)
    {
        const auto first = static_cast<uint8_t>(value[offset++]);
        uint32_t point = 0;
        unsigned remaining = 0;
        uint32_t minimum = 0;
        if (first < 0x80) point = first;
        else if (first >= 0xc2 && first <= 0xdf) { point = first & 0x1f; remaining = 1; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { point = first & 0xf; remaining = 2; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { point = first & 7; remaining = 3; minimum = 0x10000; }
        else Fail("invalid UTF-8");
        if (remaining > value.size() - offset) Fail("truncated UTF-8");
        for (unsigned index = 0; index < remaining; ++index)
        {
            const auto next = static_cast<uint8_t>(value[offset++]);
            if ((next & 0xc0) != 0x80) Fail("invalid UTF-8 continuation byte");
            point = (point << 6) | (next & 0x3f);
        }
        if (point < minimum || !XmlCharacter(point)) Fail("invalid XML character");
    }
}
void AppendPoint(std::string& output, uint32_t point)
{
    if (!XmlCharacter(point)) Fail("invalid numeric character reference");
    if (point < 0x80) output.push_back(char(point));
    else if (point < 0x800)
    {
        output.push_back(char(0xc0 | (point >> 6)));
        output.push_back(char(0x80 | (point & 0x3f)));
    }
    else if (point < 0x10000)
    {
        output.push_back(char(0xe0 | (point >> 12)));
        output.push_back(char(0x80 | ((point >> 6) & 0x3f)));
        output.push_back(char(0x80 | (point & 0x3f)));
    }
    else
    {
        output.push_back(char(0xf0 | (point >> 18)));
        output.push_back(char(0x80 | ((point >> 12) & 0x3f)));
        output.push_back(char(0x80 | ((point >> 6) & 0x3f)));
        output.push_back(char(0x80 | (point & 0x3f)));
    }
}
void ValidateLimits(const XmlLimits& limits)
{
    if (limits.maxDepth > hardDepthLimit) Fail("maxDepth cannot exceed 256");
}
const char* KindName(EntityKind kind)
{
    switch (kind)
    {
    case EntityKind::Widget: return "widget";
    case EntityKind::Screen: return "screen";
    case EntityKind::Variable: return "variable";
    case EntityKind::Project: return "project";
    case EntityKind::Style: return "style";
    case EntityKind::Font: return "font";
    case EntityKind::Asset: return "asset";
    }
    Fail("unknown entity kind");
}
EntityKind ReadKind(const std::string& kind)
{
    for (const auto value : {EntityKind::Widget, EntityKind::Screen, EntityKind::Variable,
        EntityKind::Project, EntityKind::Style, EntityKind::Font, EntityKind::Asset})
        if (kind == KindName(value)) return value;
    Fail("unknown entity kind");
}
View Trim(View value)
{
    while (!value.empty() && Space(value.front())) value.remove_prefix(1);
    while (!value.empty() && Space(value.back())) value.remove_suffix(1);
    return value;
}
bool RealToken(View value)
{
    size_t index = 0;
    if (index < value.size() && value[index] == '-') ++index;
    if (index == value.size()) return false;
    if (value[index] == '0') ++index;
    else
    {
        if (value[index] < '1' || value[index] > '9') return false;
        while (index < value.size() && value[index] >= '0' && value[index] <= '9') ++index;
    }
    if (index < value.size() && value[index] == '.')
    {
        const size_t first = ++index;
        while (index < value.size() && value[index] >= '0' && value[index] <= '9') ++index;
        if (first == index) return false;
    }
    if (index < value.size() && (value[index] == 'e' || value[index] == 'E'))
    {
        ++index;
        if (index < value.size() && (value[index] == '+' || value[index] == '-')) ++index;
        const size_t first = index;
        while (index < value.size() && value[index] >= '0' && value[index] <= '9') ++index;
        if (first == index) return false;
    }
    return index == value.size();
}
template<typename T> T Decimal(View text)
{
    if (text.empty()) Fail("empty integer");
    T value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        Fail("invalid or overflowing integer");
    return value;
}
std::string Base64(const json::binary_t& bytes)
{
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    if (bytes.size() > (std::numeric_limits<size_t>::max() - 2) / 4 * 3) Fail("binary value is too large");
    result.reserve(((bytes.size() + 2) / 3) * 4);
    for (size_t index = 0; index < bytes.size(); index += 3)
    {
        const size_t remaining = bytes.size() - index;
        const uint32_t bits = (uint32_t(bytes[index]) << 16) |
            (remaining > 1 ? uint32_t(bytes[index + 1]) << 8 : 0) |
            (remaining > 2 ? uint32_t(bytes[index + 2]) : 0);
        result += alphabet[(bits >> 18) & 63]; result += alphabet[(bits >> 12) & 63];
        result += remaining > 1 ? alphabet[(bits >> 6) & 63] : '=';
        result += remaining > 2 ? alphabet[bits & 63] : '=';
    }
    return result;
}
int Base64Digit(char ch)
{
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    if (ch == '=') return 64;
    return -1;
}
json::binary_t ReadBase64(const std::string& text)
{
    std::vector<uint8_t> bytes;
    size_t characters = 0;
    for (char ch : text) if (!Space(ch)) ++characters;
    if (characters % 4) Fail("base64 length must be a multiple of four");
    bytes.reserve(characters / 4 * 3);
    std::array<int, 4> quartet{};
    size_t used = 0;
    bool padded = false;
    for (char ch : text)
    {
        if (Space(ch)) continue;
        if (padded) Fail("base64 content after padding");
        const int digit = Base64Digit(ch);
        if (digit < 0) Fail("invalid base64 character");
        quartet[used++] = digit;
        if (used != 4) continue;
        if (quartet[0] >= 64 || quartet[1] >= 64) Fail("invalid base64 padding");
        bytes.push_back(uint8_t((quartet[0] << 2) | (quartet[1] >> 4)));
        if (quartet[2] == 64)
        {
            if (quartet[3] != 64 || (quartet[1] & 15)) Fail("invalid base64 padding bits");
            padded = true;
        }
        else
        {
            bytes.push_back(uint8_t((quartet[1] << 4) | (quartet[2] >> 2)));
            if (quartet[3] == 64)
            {
                if (quartet[2] & 3) Fail("invalid base64 padding bits");
                padded = true;
            }
            else bytes.push_back(uint8_t((quartet[2] << 6) | quartet[3]));
        }
        used = 0;
    }
    return json::binary_t(std::move(bytes));
}

struct Writer
{
    const XmlLimits& limits;
    std::string output;
    size_t nodes = 0;
    explicit Writer(const XmlLimits& configuration) : limits(configuration) {}
    void Append(View text)
    {
        if (output.size() > limits.maxBytes || text.size() > limits.maxBytes - output.size())
            Fail("document exceeds maxBytes");
        output.append(text.data(), text.size());
    }
    void Character(char ch) { Append(View(&ch, 1)); }
    void Node()
    {
        if (nodes >= limits.maxNodes) Fail("document exceeds maxNodes");
        ++nodes;
    }
    void Indent(size_t amount) { for (size_t index = 0; index < amount; ++index) Append("  "); }
    void Escaped(const std::string& text)
    {
        if (text.size() > limits.maxTextBytes) Fail("text exceeds maxTextBytes");
        if (output.size() > limits.maxBytes || text.size() > limits.maxBytes - output.size())
            Fail("document exceeds maxBytes");
        ValidateText(text);
        for (char ch : text)
        {
            switch (ch)
            {
            case '&': Append("&amp;"); break;
            case '<': Append("&lt;"); break;
            case '>': Append("&gt;"); break;
            case '"': Append("&quot;"); break;
            case '\'': Append("&apos;"); break;
            case '\t': Append("&#x9;"); break;
            case '\n': Append("&#xA;"); break;
            case '\r': Append("&#xD;"); break;
            default: Character(ch); break;
            }
        }
    }
    void Scalar(const char* tag, const std::string& value, size_t indent)
    {
        Indent(indent); Append("<"); Append(tag); Append(">");
        Escaped(value); Append("</"); Append(tag); Append(">\n");
    }
    void Value(const json& value, size_t depth, size_t indent)
    {
        if (depth > limits.maxDepth) Fail("document exceeds maxDepth");
        Node();
        if (FontData::Matches(value))
        {
            Indent(indent); Append("<font format_version=\"1\">\n");
            Value(FontData::ToXML(value), depth + 1, indent + 1);
            Indent(indent); Append("</font>\n");
        }
        else if (value.is_object())
        {
            Indent(indent);
            if (value.empty()) { Append("<object/>\n"); return; }
            Append("<object>\n");
            for (const auto& property : value.items())
            {
                Node(); Indent(indent + 1); Append("<property name=\""); Escaped(property.key()); Append("\">\n");
                Value(property.value(), depth + 1, indent + 2);
                Indent(indent + 1); Append("</property>\n");
            }
            Indent(indent); Append("</object>\n");
        }
        else if (value.is_array())
        {
            Indent(indent);
            if (value.empty()) { Append("<array/>\n"); return; }
            Append("<array>\n");
            for (const auto& child : value) Value(child, depth + 1, indent + 1);
            Indent(indent); Append("</array>\n");
        }
        else if (value.is_string()) Scalar("string", value.get_ref<const std::string&>(), indent);
        else if (value.is_null()) { Indent(indent); Append("<null/>\n"); }
        else if (value.is_boolean()) Scalar("boolean", value.get<bool>() ? "true" : "false", indent);
        else if (value.is_number_unsigned()) Scalar("unsigned", std::to_string(value.get<uint64_t>()), indent);
        else if (value.is_number_integer()) Scalar("integer", std::to_string(value.get<int64_t>()), indent);
        else if (value.is_number_float())
        {
            if (!std::isfinite(value.get<double>())) Fail("non-finite real value");
            Scalar("real", value.dump(), indent);
        }
        else if (value.is_binary())
        {
            const auto& bytes = value.get_binary();
            if (limits.maxTextBytes < 6) Fail("binary encoding attribute exceeds maxTextBytes");
            if (bytes.size() / 3 > limits.maxTextBytes / 4 ||
                ((bytes.size() + 2) / 3) * 4 > limits.maxTextBytes) Fail("binary text exceeds maxTextBytes");
            Indent(indent); Append("<binary encoding=\"base64\"");
            if (bytes.has_subtype()) { Append(" subtype=\""); Escaped(std::to_string(bytes.subtype())); Append("\""); }
            Append(">"); Append(Base64(bytes)); Append("</binary>\n");
        }
        else Fail("unsupported JSON value type");
    }
};

struct Tag
{
    std::string name;
    std::map<std::string, std::string> attributes;
    bool empty = false;
};
class Reader
{
    View input;
    const XmlLimits& limits;
    size_t offset = 0;
    size_t nodes = 0;
    bool Starts(View text) const { return input.substr(offset, text.size()) == text; }
    void Require(View text)
    {
        if (!Starts(text)) Fail("expected '" + std::string(text) + "' at byte " + std::to_string(offset));
        offset += text.size();
    }
    bool SkipSpace()
    {
        const size_t start = offset;
        while (offset < input.size() && Space(input[offset])) ++offset;
        return offset != start;
    }
    void Comment()
    {
        Require("<!--");
        const size_t end = input.find("-->", offset);
        if (end == View::npos) Fail("unterminated comment");
        if (input.substr(offset, end - offset).find("--") != View::npos ||
            (end > offset && input[end - 1] == '-')) Fail("invalid XML comment");
        offset = end + 3;
    }
    void Trivia()
    {
        for (;;) { SkipSpace(); if (!Starts("<!--")) return; Comment(); }
    }
    std::string Name()
    {
        const size_t start = offset;
        const auto first = [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_'; };
        if (offset >= input.size() || !first(input[offset])) Fail("invalid XML name");
        ++offset;
        while (offset < input.size() && (first(input[offset]) || (input[offset] >= '0' && input[offset] <= '9') ||
            input[offset] == '-' || input[offset] == '.')) ++offset;
        if (offset < input.size() && input[offset] == ':') Fail("XML namespaces are not supported");
        if (offset - start > 64) Fail("XML tag/attribute name is too long");
        return std::string(input.substr(start, offset - start));
    }
    void Entity(std::string& output)
    {
        Require("&");
        const size_t end = input.find(';', offset);
        if (end == View::npos) Fail("invalid XML entity");
        const auto entity = input.substr(offset, end - offset);
        offset = end + 1;
        if (entity == "amp") output += '&';
        else if (entity == "lt") output += '<';
        else if (entity == "gt") output += '>';
        else if (entity == "quot") output += '"';
        else if (entity == "apos") output += '\'';
        else if (!entity.empty() && entity[0] == '#')
        {
            auto number = entity.substr(1);
            int base = 10;
            if (!number.empty() && number[0] == 'x') { base = 16; number.remove_prefix(1); }
            if (number.empty() || number[0] == '+' || number[0] == '-') Fail("invalid character reference");
            uint32_t point = 0;
            const auto parsed = std::from_chars(number.data(), number.data() + number.size(), point, base);
            if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()) Fail("invalid character reference");
            AppendPoint(output, point);
        }
        else Fail("unknown/external XML entity");
    }
    std::string Text(char quote = 0)
    {
        std::string result;
        while (offset < input.size())
        {
            const char ch = input[offset];
            if (quote && ch == quote) return result;
            if (ch == '<')
            {
                if (quote) Fail("unescaped '<' in attribute");
                if (Starts("<!--")) { Comment(); continue; }
                return result;
            }
            if (!quote && Starts("]]>")) Fail("unescaped ']]>' in text");
            if (ch == '&') Entity(result);
            else
            {
                ++offset;
                if (ch == '\r')
                {
                    if (offset < input.size() && input[offset] == '\n') ++offset;
                    result += quote ? ' ' : '\n';
                }
                else result += quote && (ch == '\n' || ch == '\t') ? ' ' : ch;
            }
            if (result.size() > limits.maxTextBytes) Fail("text exceeds maxTextBytes");
        }
        if (quote) Fail("unterminated attribute");
        return result;
    }
    std::pair<std::string, std::string> Attribute()
    {
        auto name = Name(); SkipSpace(); Require("="); SkipSpace();
        if (offset >= input.size() || (input[offset] != '\'' && input[offset] != '"')) Fail("attribute must be quoted");
        const char quote = input[offset++];
        auto value = Text(quote); Require(View(&quote, 1));
        return {std::move(name), std::move(value)};
    }
    Tag Open()
    {
        Require("<");
        if (Starts("!") || Starts("?")) Fail("DOCTYPE, declarations and processing instructions are forbidden here");
        Tag tag; tag.name = Name();
        if (nodes >= limits.maxNodes) Fail("document exceeds maxNodes");
        ++nodes;
        for (;;)
        {
            const bool separated = SkipSpace();
            if (Starts("/>")) { offset += 2; tag.empty = true; return tag; }
            if (Starts(">")) { ++offset; return tag; }
            if (!separated) Fail("attributes must be separated by whitespace");
            auto attribute = Attribute();
            if (!tag.attributes.emplace(std::move(attribute)).second) Fail("duplicate XML attribute");
        }
    }
    void Attributes(const Tag& tag, std::initializer_list<const char*> allowed)
    {
        for (const auto& attribute : tag.attributes)
        {
            bool found = false;
            for (const char* name : allowed) if (attribute.first == name) { found = true; break; }
            if (!found) Fail("unknown attribute '" + attribute.first + "' on " + tag.name);
        }
    }
    const std::string& Required(const Tag& tag, const char* name)
    {
        const auto found = tag.attributes.find(name);
        if (found == tag.attributes.end()) Fail("missing attribute '" + std::string(name) + "' on " + tag.name);
        return found->second;
    }
    bool Closing(const std::string& name) const
    {
        if (!Starts("</")) return false;
        const auto next = input.substr(offset + 2, name.size());
        if (next != name) return false;
        const size_t after = offset + 2 + name.size();
        return after < input.size() && (input[after] == '>' || Space(input[after]));
    }
    void Close(const std::string& name)
    {
        Require("</");
        if (Name() != name) Fail("mismatched closing tag");
        SkipSpace(); Require(">");
    }
    std::string Scalar(const Tag& tag)
    {
        if (tag.empty) return {};
        auto text = Text(); Close(tag.name); return text;
    }
    void Declaration()
    {
        Require("<?xml");
        if (!SkipSpace()) Fail("invalid XML declaration");
        auto version = Attribute();
        if (version.first != "version" || version.second != "1.0") Fail("only XML version 1.0 is supported");
        bool encoding = false, standalone = false;
        for (;;)
        {
            const bool separated = SkipSpace();
            if (Starts("?>")) { offset += 2; return; }
            if (!separated) Fail("invalid XML declaration");
            auto attribute = Attribute();
            if (attribute.first == "encoding" && !encoding && !standalone)
            {
                for (char& ch : attribute.second) if (ch >= 'a' && ch <= 'z') ch = char(ch - 'a' + 'A');
                if (attribute.second != "UTF-8") Fail("only UTF-8 XML is supported");
                encoding = true;
            }
            else if (attribute.first == "standalone" && !standalone && (attribute.second == "yes" || attribute.second == "no")) standalone = true;
            else Fail("unknown, duplicate or unordered XML declaration attribute");
        }
    }
    json Value(size_t depth)
    {
        if (depth > limits.maxDepth) Fail("document exceeds maxDepth");
        const auto tag = Open();
        if (tag.name == "font")
        {
            Attributes(tag, {"format_version"});
            if (Required(tag, "format_version") != "1" || tag.empty) Fail("invalid font format_version or missing data");
            Trivia(); auto value = Value(depth + 1); Trivia(); Close("font");
            return FontData::FromXML(std::move(value));
        }
        if (tag.name == "object")
        {
            Attributes(tag, {});
            json result = json::object();
            if (tag.empty) return result;
            for (;;)
            {
                Trivia();
                if (Closing("object")) { Close("object"); return result; }
                const auto property = Open();
                if (property.name != "property") Fail("objects contain property elements only");
                Attributes(property, {"name"}); const auto& key = Required(property, "name");
                if (property.empty) Fail("a property needs exactly one typed value");
                if (result.contains(key)) Fail("duplicate property name");
                Trivia(); auto value = Value(depth + 1); Trivia(); Close("property");
                result.emplace(key, std::move(value));
            }
        }
        if (tag.name == "array")
        {
            Attributes(tag, {});
            json result = json::array();
            if (tag.empty) return result;
            for (;;)
            {
                Trivia();
                if (Closing("array")) { Close("array"); return result; }
                result.push_back(Value(depth + 1));
            }
        }
        if (tag.name == "binary")
        {
            Attributes(tag, {"encoding", "subtype"});
            if (Required(tag, "encoding") != "base64") Fail("binary encoding must be base64");
            auto data = ReadBase64(Scalar(tag));
            const auto subtype = tag.attributes.find("subtype");
            if (subtype != tag.attributes.end()) data.set_subtype(Decimal<uint64_t>(subtype->second));
            return json(std::move(data));
        }
        Attributes(tag, {});
        if (tag.name == "string") return Scalar(tag);
        const auto body = Scalar(tag);
        const auto text = Trim(body);
        if (tag.name == "null") { if (!text.empty()) Fail("null must be empty"); return nullptr; }
        if (tag.name == "boolean")
        {
            if (text == "true") return true;
            if (text == "false") return false;
            Fail("boolean must be true or false");
        }
        if (tag.name == "integer") return json(Decimal<int64_t>(text));
        if (tag.name == "unsigned") return json(Decimal<uint64_t>(text));
        if (tag.name == "real")
        {
            // Only a number reaches the existing locale-independent JSON
            // numeric converter. In particular, nested JSON cannot bypass the
            // XML node/depth budgets through a scalar's text.
            if (!RealToken(text)) Fail("invalid real number token");
            try
            {
                auto number = json::parse(text.begin(), text.end());
                if (!number.is_number()) Fail("real must be a finite number");
                auto value = number.get<double>();
                if (!std::isfinite(value)) Fail("non-finite or overflowing real value");
                if (value == 0)
                {
                    const auto mantissa = text.substr(0, text.find_first_of("eE"));
                    if (mantissa.find_first_of("123456789") != View::npos) Fail("real value underflow");
                    if (!text.empty() && text.front() == '-') value = -0.0;
                }
                return json(value);
            }
            catch (const json::exception&) { Fail("invalid or overflowing real value"); }
        }
        Fail("unknown typed value element '" + tag.name + "'");
    }
public:
    Reader(View bytes, const XmlLimits& configuration) : input(bytes), limits(configuration) {}
    BinaryRecord Parse()
    {
        if (Starts(View("\xef\xbb\xbf", 3))) offset += 3;
        if (Starts("<?xml")) Declaration();
        Trivia();
        const auto root = Open();
        if (root.name != "uicreator") Fail("expected the uicreator root element");
        Attributes(root, {"format_version", "kind", "type"});
        if (Required(root, "format_version") != "1") Fail("unsupported format_version");
        const auto kind = ReadKind(Required(root, "kind"));
        const auto type = Required(root, "type");
        if (type.empty()) Fail("entity type must not be empty");
        if (root.empty) Fail("the root needs exactly one typed value");
        Trivia(); auto value = Value(0); Trivia(); Close("uicreator"); Trivia();
        if (offset != input.size()) Fail("trailing data after the root element");
        return {kind, type, std::move(value), {}};
    }
};
}

std::string Xml::Encode(EntityKind kind, const std::string& type, const nlohmann::json& data, const XmlLimits& limits)
{
    ValidateLimits(limits);
    if (type.empty()) Fail("entity type must not be empty");
    if (limits.maxTextBytes < 5) Fail("UTF-8 encoding attribute exceeds maxTextBytes");
    Writer writer(limits); writer.Node();
    writer.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<uicreator format_version=\"1\" kind=\"");
    writer.Escaped(KindName(kind)); writer.Append("\" type=\""); writer.Escaped(type); writer.Append("\">\n");
    writer.Value(data, 0, 1); writer.Append("</uicreator>\n");
    return std::move(writer.output);
}
BinaryRecord Xml::Decode(const void* data, size_t size, const XmlLimits& limits)
{
    ValidateLimits(limits);
    if (size > limits.maxBytes) Fail("document exceeds maxBytes");
    if (!data || !size) Fail("document is empty or has a null input pointer");
    const View bytes(static_cast<const char*>(data), size);
    ValidateText(bytes);
    return Reader(bytes, limits).Parse();
}
BinaryRecord Xml::Decode(const std::string& data, const XmlLimits& limits)
{
    return Decode(data.data(), data.size(), limits);
}
}
