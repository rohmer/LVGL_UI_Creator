#include "FigmaOnline.h"
#include <Serialization/Base64.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace FigmaOnline {
namespace {
constexpr size_t MaxJsonBytes = 64u * 1024u * 1024u;
constexpr size_t MaxImageBytes = 24u * 1024u * 1024u;
constexpr uint64_t MaxImagePixels = 16u * 1024u * 1024u;
constexpr uint64_t MaxTotalPixels = 64u * 1024u * 1024u;
constexpr size_t MaxImages = 1024, BatchSize = 50;

void Cancelled(const std::atomic<bool>* cancel) {
    if(cancel && cancel->load()) throw std::runtime_error("Figma import cancelled.");
}
void Report(const Progress& progress, const std::string& message) { if(progress) progress(message); }
std::string Lower(std::string value) {
    for(auto& ch : value) if(ch >= 'A' && ch <= 'Z') ch = char(ch + ('a' - 'A'));
    return value;
}
bool AsciiAlphaNumeric(unsigned char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}
void FileKey(const std::string& key) {
    if(key.empty() || key.size() > 256 || !std::all_of(key.begin(), key.end(), [](unsigned char ch) {
        return AsciiAlphaNumeric(ch) || ch == '_' || ch == '-';
    })) throw std::invalid_argument("The Figma link does not contain a valid file key.");
}
void Token(const std::string& token) {
    if(token.empty()) throw std::invalid_argument("Enter a Figma personal access token with file_content:read permission.");
    if(token.size() > 4096 || !std::all_of(token.begin(), token.end(), [](unsigned char ch) { return ch >= 33 && ch <= 126; }))
        throw std::invalid_argument("The Figma token contains invalid characters. Paste only the token, without spaces or line breaks.");
}
int Hex(unsigned char ch) {
    if(ch >= '0' && ch <= '9') return ch - '0';
    if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}
std::string Decode(const std::string& text) {
    std::string result;
    for(size_t i = 0; i < text.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if(ch == '%') {
            if(i + 2 >= text.size() || Hex(text[i + 1]) < 0 || Hex(text[i + 2]) < 0)
                throw std::invalid_argument("The Figma link has invalid URL encoding.");
            ch = static_cast<unsigned char>((Hex(text[i + 1]) << 4) | Hex(text[i + 2]));
            i += 2;
        } else if(ch == '+') ch = ' ';
        if(ch < 32 || ch == 127) throw std::invalid_argument("The Figma link contains invalid characters.");
        result.push_back(static_cast<char>(ch));
    }
    return result;
}
std::string Encode(const std::string& text) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string output;
    for(const unsigned char ch : text) {
        if(AsciiAlphaNumeric(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') output.push_back(static_cast<char>(ch));
        else { output.push_back('%'); output.push_back(hex[ch >> 4]); output.push_back(hex[ch & 15]); }
    }
    return output;
}
void NodeId(const std::string& id) {
    if(id.empty() || id.size() > 256 || !std::all_of(id.begin(), id.end(), [](unsigned char ch) {
        return AsciiAlphaNumeric(ch) || ch == ':' || ch == ';' || ch == '_' || ch == '-';
    })) throw std::invalid_argument("The Figma link does not contain a valid node ID.");
}
std::string Header(const HttpResponse& response, const std::string& name) {
    for(const auto& header : response.headers) if(Lower(header.first) == name) return header.second;
    return {};
}
HttpResponse Request(const HttpRequest& request, const std::atomic<bool>* cancel, const Transport& transport) {
    Cancelled(cancel);
    HttpResponse response;
    try { response = transport ? transport(request, cancel) : Get(request, cancel); }
    catch(const std::exception& error) {
        Cancelled(cancel);
        // Transport diagnostics should already exclude credentials. Guard even
        // injected transports so API tokens cannot escape through an error.
        const std::string message = error.what();
        if(!request.token.empty() && message.find(request.token) != std::string::npos)
            throw std::runtime_error("The Figma network request failed.");
        throw;
    }
    Cancelled(cancel);
    if(response.body.size() > request.maxBytes) throw std::runtime_error("The Figma response exceeds the import size limit.");
    if(response.status >= 200 && response.status < 300) return response;
    if(response.status == 401 || response.status == 403)
        throw std::runtime_error("Figma access was denied. Check that the token is valid, has file_content:read permission, and its account can access this design.");
    if(response.status == 404) throw std::runtime_error("The Figma design or selected node was not found. Check the link and file access.");
    if(response.status == 429) {
        const auto retry = Header(response, "retry-after");
        std::string message = "Figma's API rate limit was reached.";
        if(!retry.empty() && retry.size() <= 8 && std::all_of(retry.begin(), retry.end(), [](unsigned char ch) { return ch >= '0' && ch <= '9'; }))
            message += " Retry after " + retry + " seconds.";
        else message += " Try again later.";
        throw std::runtime_error(message + " Limits depend on your Figma plan and seat; no automatic retries were made.");
    }
    if(response.status >= 500) throw std::runtime_error("Figma's service is temporarily unavailable. Try again later.");
    throw std::runtime_error("Figma could not complete the request (HTTP " + std::to_string(response.status) + ").");
}
json Parse(const std::string& body) {
    try {
        auto result = json::parse(body, [](int depth, json::parse_event_t, json&) {
            if(depth > 128) throw std::invalid_argument("Figma JSON exceeds the nesting limit.");
            return true;
        });
        if(!result.is_object()) throw std::invalid_argument("Figma JSON must contain an object.");
        return result;
    } catch(const std::exception&) { throw std::runtime_error("Figma returned an invalid or overly nested JSON response."); }
}
const json& Member(const json& value, const char* key) {
    static const json empty = json::object();
    const auto found = value.find(key);
    return found == value.end() ? empty : *found;
}
std::string Text(const json& value, const char* key, const std::string& fallback = {}) {
    const auto found = value.find(key);
    if(found == value.end()) return fallback;
    if(!found->is_string()) throw std::runtime_error("Figma returned an invalid layer property.");
    const auto result = found->get<std::string>();
    if(result.find('\0') != std::string::npos) throw std::runtime_error("Figma returned an invalid layer property.");
    return result;
}
double Number(const json& value, const char* key, double fallback = 0) {
    const auto found = value.find(key);
    if(found == value.end()) return fallback;
    if(!found->is_number()) throw std::runtime_error("Figma returned an invalid layer property.");
    const auto result = found->get<double>();
    if(!std::isfinite(result)) throw std::runtime_error("Figma returned invalid layer geometry.");
    return result;
}
bool Visible(const json& value) {
    const auto found = value.find("visible");
    if(found == value.end()) return true;
    if(!found->is_boolean()) throw std::runtime_error("Figma returned an invalid visibility property.");
    return found->get<bool>();
}
bool Flag(const json& value, const char* key) {
    const auto found = value.find(key);
    if(found == value.end()) return false;
    if(!found->is_boolean()) throw std::runtime_error("Figma returned an invalid layer property.");
    return found->get<bool>();
}
std::vector<const json*> Paints(const json& node, const char* key) {
    std::vector<const json*> result;
    const auto found = node.find(key);
    if(found == node.end()) return result;
    if(!found->is_array()) throw std::runtime_error("Figma returned an invalid paint collection.");
    for(const auto& paint : *found) {
        if(!paint.is_object()) throw std::runtime_error("Figma returned an invalid paint.");
        if(Visible(paint)) result.push_back(&paint);
    }
    return result;
}
bool Structural(const std::string& type) { return type == "DOCUMENT" || type == "CANVAS" || type == "SECTION"; }
struct IndexedNode { const json* value; bool hidden; };
std::vector<const json*> Roots(const json& document) {
    std::vector<const json*> roots;
    if(document.contains("document")) roots.push_back(&document.at("document"));
    else if(document.contains("nodes")) for(const auto& node : document.at("nodes").items()) {
        if(node.value().is_object() && node.value().contains("document")) roots.push_back(&node.value().at("document"));
    }
    else if(document.contains("type")) roots.push_back(&document);
    return roots;
}
void Index(const json& node, std::map<std::string, IndexedNode>& nodes, bool hidden = false, size_t depth = 0) {
    if(depth > 64 || nodes.size() > 50000) throw std::runtime_error("The Figma document exceeds the layer limit.");
    hidden = hidden || !Visible(node);
    nodes.emplace(Text(node, "id"), IndexedNode{&node, hidden});
    if(node.contains("children")) for(const auto& child : node.at("children")) Index(child, nodes, hidden, depth + 1);
}
std::vector<std::string> RasterReasons(const json& node) {
    static const std::set<std::string> native = {"FRAME", "COMPONENT", "COMPONENT_SET", "INSTANCE", "GROUP", "RECTANGLE", "ELLIPSE", "TEXT", "LINE"};
    const auto type = Text(node, "type");
    std::vector<std::string> reasons;
    if(!native.count(type)) reasons.push_back("vector or unsupported artwork");
    const auto fills = Paints(node, "fills"), strokes = Paints(node, "strokes");
    if(std::any_of(fills.begin(), fills.end(), [](const json* paint) { return Text(*paint, "type") != "SOLID"; })) reasons.push_back("image or gradient fill");
    if(fills.size() > 1) reasons.push_back("multiple fills");
    if(strokes.size() > 1 || std::any_of(strokes.begin(), strokes.end(), [](const json* paint) { return Text(*paint, "type") != "SOLID"; })) reasons.push_back("complex stroke");
    if(!Paints(node, "effects").empty()) reasons.push_back("effects");
    if(std::abs(std::remainder(Number(node, "rotation"), 360.0)) > 0.01) reasons.push_back("rotation");
    const auto& transform = Member(node, "relativeTransform");
    if(transform.is_array() && transform.size() >= 2 && transform[0].is_array() && transform[1].is_array() && transform[0].size() >= 2 && transform[1].size() >= 2) {
        if(!transform[0][0].is_number() || !transform[0][1].is_number() || !transform[1][0].is_number() || !transform[1][1].is_number())
            throw std::runtime_error("Figma returned an invalid layer transform.");
        const double a = transform[0][0], b = transform[0][1], c = transform[1][0], d = transform[1][1];
        if(a < 0 || d < 0 || std::abs(b) > 0.00001 || std::abs(c) > 0.00001) reasons.push_back("rotation or reflection");
    }
    const auto blend = Text(node, "blendMode", "NORMAL");
    if(blend != "NORMAL" && blend != "PASS_THROUGH") reasons.push_back("blend mode");
    if(node.contains("children")) for(const auto& child : node.at("children"))
        if(Flag(child, "isMask") && Visible(child)) { reasons.push_back("masked group"); break; }
    if(node.contains("strokeDashes") && node.at("strokeDashes").is_array() && !node.at("strokeDashes").empty()) reasons.push_back("dashed stroke");
    if(Number(node, "cornerSmoothing") > 0.00001) reasons.push_back("smoothed corners");
    if(node.contains("rectangleCornerRadii")) {
        const auto& radii = node.at("rectangleCornerRadii");
        if(radii.is_array() && radii.size() == 4 && std::any_of(radii.begin(), radii.end(), [&](const json& radius) { return radius != radii[0]; })) reasons.push_back("unequal corner radii");
    }
    if(type == "ELLIPSE") {
        const auto& bounds = Member(node, "absoluteBoundingBox");
        if(std::abs(Number(bounds, "width") - Number(bounds, "height")) > 0.01) reasons.push_back("non-circular ellipse");
        const auto& arc = Member(node, "arcData");
        if(arc.is_object() && !arc.empty() && (Number(arc, "innerRadius") > 0 || std::abs(Number(arc, "endingAngle") - Number(arc, "startingAngle") - 6.283185307179586) > 0.01))
            reasons.push_back("partial ellipse");
    }
    if(type == "TEXT") {
        const auto& style = Member(node, "style");
        if(Text(style, "textDecoration", "NONE") != "NONE") reasons.push_back("text decoration");
        if(node.contains("characterStyleOverrides") && node.at("characterStyleOverrides").is_array())
            if(std::any_of(node.at("characterStyleOverrides").begin(), node.at("characterStyleOverrides").end(), [](const json& value) { return value != 0; })) reasons.push_back("mixed text styles");
        if(Text(style, "textCase", "ORIGINAL") != "ORIGINAL") reasons.push_back("text case");
    }
    return reasons;
}
std::string Join(const std::vector<std::string>& values, const std::string& separator) {
    std::string result;
    for(const auto& value : values) { if(!result.empty()) result += separator; result += value; }
    return result;
}
struct Render { const json* node; std::string id, reason; double scale; };
double RenderScale(const json& node, double scale) {
    const auto& bounds = Member(node, "absoluteBoundingBox");
    const auto width = Number(bounds, "width"), height = Number(bounds, "height");
    if(width <= 0 || height <= 0) return 0;
    auto result = std::min({scale, 4.0, 4096.0 / std::max(width, height), std::sqrt(double(MaxImagePixels) / (width * height))});
    if(!std::isfinite(result) || result < 0.01) throw std::runtime_error("A Figma artwork layer is too large to render. Reduce its dimensions in Figma.");
    // The API accepts one scale per batch. Quantization also makes repeated
    // reviews compare cache quality without floating point drift.
    return std::floor(result * 1000000.0) / 1000000.0;
}
std::string Scale(double scale) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(6) << std::fixed << scale;
    return stream.str();
}
bool Cached(const json& assets, const std::string& id, double scale) {
    const auto found = assets.find(id);
    return found != assets.end() && found->is_object() && found->contains("data") && (*found)["data"].is_string() &&
        !(*found)["data"].get_ref<const std::string&>().empty() && Number(*found, "scale", 1.0) + 0.0000001 >= scale;
}
void Collect(const json& node, bool hidden, const FigmaImport::Options& options, const json& assets,
             std::vector<Render>& renders, std::set<std::string>& visited) {
    hidden = hidden || !Visible(node);
    if(hidden && !options.includeHidden) return;
    const auto type = Text(node, "type"), id = Text(node, "id");
    if(!visited.insert(id).second || type == "SLICE") return;
    if(!Structural(type)) {
        const auto reasons = RasterReasons(node);
        if(!reasons.empty()) {
            if(!Member(node, "absoluteBoundingBox").is_object()) return; // Invisible nodes may have null bounds.
            const double scale = RenderScale(node, options.scale);
            if(scale == 0) return;
            if(!Cached(assets, id, scale)) renders.push_back({&node, id, Join(reasons, ", "), scale});
            return; // This render contains the entire subtree.
        }
    }
    if(node.contains("children")) for(const auto& child : node.at("children")) Collect(child, hidden, options, assets, renders, visited);
}
uint32_t BigEndian(const std::string& bytes, size_t index) {
    return (uint32_t(static_cast<unsigned char>(bytes[index])) << 24) | (uint32_t(static_cast<unsigned char>(bytes[index + 1])) << 16) |
           (uint32_t(static_cast<unsigned char>(bytes[index + 2])) << 8) | uint32_t(static_cast<unsigned char>(bytes[index + 3]));
}
uint64_t PngPixels(const std::string& bytes) {
    static const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if(bytes.size() < 33 || !std::equal(std::begin(signature), std::end(signature), bytes.begin(), [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); }) ||
       BigEndian(bytes, 8) != 13 || bytes.compare(12, 4, "IHDR") != 0) throw std::runtime_error("Figma returned invalid PNG artwork.");
    const auto width = BigEndian(bytes, 16), height = BigEndian(bytes, 20);
    const auto pixels = uint64_t(width) * height;
    if(!width || !height || width > 4096 || height > 4096 || pixels > MaxImagePixels) throw std::runtime_error("Figma artwork exceeds the image dimension limit.");
    return pixels;
}
void AddWarning(json& result, const std::string& warning) {
    if(!result.contains("uicreatorExportWarnings")) result["uicreatorExportWarnings"] = json::array();
    auto& warnings = result["uicreatorExportWarnings"];
    if(!warnings.is_array()) throw std::runtime_error("The Figma document contains invalid conversion notes.");
    if(std::find(warnings.begin(), warnings.end(), warning) == warnings.end() && warnings.size() < 200) warnings.push_back(warning);
}
}

Location ParseUrl(const std::string& link) {
    if(link.size() > 8192) throw std::invalid_argument("The Figma design link is too long.");
    const auto first = link.find_first_not_of(" \t\r\n");
    const auto last = link.find_last_not_of(" \t\r\n");
    const auto value = first == std::string::npos ? std::string{} : link.substr(first, last - first + 1);
    if(value.size() < 8 || Lower(value.substr(0, 8)) != "https://" || std::any_of(value.begin(), value.end(), [](unsigned char ch) { return ch <= 32 || ch >= 127 || ch == '\\'; }))
        throw std::invalid_argument("Paste an HTTPS Figma design link, such as https://www.figma.com/design/FILE_KEY/Design.");
    const auto hostEnd = value.find_first_of("/?#", 8);
    const auto host = Lower(value.substr(8, hostEnd == std::string::npos ? std::string::npos : hostEnd - 8));
    if(host != "www.figma.com" && host != "figma.com") throw std::invalid_argument("Only design links from figma.com are supported.");
    if(hostEnd == std::string::npos || value[hostEnd] != '/') throw std::invalid_argument("The Figma link must identify a design file.");
    const auto pathEnd = value.find_first_of("?#", hostEnd);
    const auto path = value.substr(hostEnd + 1, pathEnd == std::string::npos ? std::string::npos : pathEnd - hostEnd - 1);
    const auto typeEnd = path.find('/');
    if(typeEnd == std::string::npos) throw std::invalid_argument("The Figma link must identify a design file.");
    const auto type = path.substr(0, typeEnd);
    if(type != "design" && type != "file" && type != "proto") throw std::invalid_argument("Use a Figma design, file, or prototype link.");
    const auto keyEnd = path.find('/', typeEnd + 1);
    Location result{Decode(path.substr(typeEnd + 1, keyEnd == std::string::npos ? std::string::npos : keyEnd - typeEnd - 1)), {}};
    FileKey(result.fileKey);
    const auto queryStart = value.find('?', hostEnd);
    const auto fragment = value.find('#', hostEnd);
    if(queryStart != std::string::npos && (fragment == std::string::npos || queryStart < fragment)) {
        const auto query = value.substr(queryStart + 1, fragment == std::string::npos ? std::string::npos : fragment - queryStart - 1);
        bool seenNodeId = false;
        for(size_t start = 0; start < query.size();) {
            const auto end = query.find('&', start);
            const auto pair = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
            const auto equals = pair.find('=');
            if(Decode(pair.substr(0, equals)) == "node-id") {
                if(seenNodeId || equals == std::string::npos) throw std::invalid_argument("The Figma link has an invalid node selection.");
                seenNodeId = true;
                result.nodeId = Decode(pair.substr(equals + 1));
                std::replace(result.nodeId.begin(), result.nodeId.end(), '-', ':');
                NodeId(result.nodeId);
            }
            if(end == std::string::npos) break;
            start = end + 1;
        }
    }
    return result;
}
Design Load(const std::string& link, const std::string& token, const std::atomic<bool>* cancel, const Progress& progress, const Transport& transport) {
    const auto location = ParseUrl(link);
    Token(token);
    Cancelled(cancel);
    Report(progress, "Connecting to Figma and reading design frames...");
    std::string url = "https://api.figma.com/v1/files/" + Encode(location.fileKey);
    if(!location.nodeId.empty()) url += "/nodes?ids=" + Encode(location.nodeId) + "&geometry=paths";
    else url += "?geometry=paths";
    auto document = Parse(Request({url, token, MaxJsonBytes}, cancel, transport).body);
    // Validates shape, IDs, geometry and document limits before the UI offers
    // any frames; parse errors and API response bodies never reach diagnostics.
    try { FigmaImport::Frames(document); }
    catch(const std::exception&) { throw std::runtime_error("Figma returned an invalid, empty, or unsupported design document."); }
    document["uicreatorFigmaVersion"] = 1;
    Report(progress, "Figma frames loaded. Select frames and choose Review conversion to download their artwork.");
    return {location, std::move(document)};
}
json AddRenders(const Design& design, const std::string& token, const FigmaImport::Options& options, const std::atomic<bool>* cancel, const Progress& progress, const Transport& transport) {
    FileKey(design.location.fileKey);
    if(!std::isfinite(options.scale) || options.scale < 0.001 || options.scale > 100) throw std::invalid_argument("Choose a valid Figma import scale.");
    Cancelled(cancel);
    const auto frames = FigmaImport::Frames(design.document);
    std::map<std::string, IndexedNode> nodes;
    for(const auto* root : Roots(design.document)) Index(*root, nodes);
    json result = design.document;
    result["uicreatorFigmaVersion"] = 1;
    if(!result.contains("uicreatorAssets")) result["uicreatorAssets"] = json::object();
    auto& assets = result["uicreatorAssets"];
    if(!assets.is_object()) throw std::runtime_error("The Figma document contains invalid embedded artwork.");
    auto selected = options.nodeIds;
    if(selected.empty()) for(const auto& frame : frames) selected.push_back(frame.id);
    std::vector<Render> renders;
    std::set<std::string> selectedIds, visited;
    for(const auto& id : selected) {
        NodeId(id);
        if(!selectedIds.insert(id).second) throw std::invalid_argument("A Figma frame was selected more than once.");
        const auto found = nodes.find(id);
        if(found == nodes.end() || Structural(Text(*found->second.value, "type"))) throw std::invalid_argument("The selected Figma frame is missing.");
        Collect(*found->second.value, found->second.hidden, options, assets, renders, visited);
    }
    if(renders.empty()) return result;
    Token(token);
    size_t imageCount = assets.size();
    for(const auto& render : renders) if(!assets.contains(render.id)) ++imageCount;
    if(imageCount > MaxImages) throw std::runtime_error("The Figma import contains too many rendered images. Select fewer frames.");
    size_t downloaded = 0;
    uint64_t pixels = 0;
    std::map<std::string, std::pair<size_t, uint64_t>> cachedSizes;
    for(const auto& item : assets.items()) {
        const auto& asset = item.value();
        if(!asset.is_object() || !asset.contains("data") || !asset["data"].is_string())
            throw std::runtime_error("The Figma document contains invalid cached artwork.");
        const auto& encoded = asset["data"].get_ref<const std::string&>();
        if(encoded.size() % 4 || encoded.size() > (MaxImageBytes - downloaded) * 4 / 3 + 4)
            throw std::runtime_error("Embedded Figma images exceed the 24 MiB import limit.");
        const auto png = base64_decode(encoded);
        if(png.size() > MaxImageBytes - downloaded) throw std::runtime_error("Embedded Figma images exceed the 24 MiB import limit.");
        const auto count = PngPixels(png);
        if(count > MaxTotalPixels - pixels) throw std::runtime_error("Cached Figma artwork exceeds the decoded image limit. Reload the design and select fewer frames.");
        cachedSizes[item.key()] = {png.size(), count};
        downloaded += png.size();
        pixels += count;
    }
    const auto version = Text(result, "version");
    if(version.empty()) AddWarning(result, "Figma did not provide a file version; artwork was rendered from the current design revision.");
    std::map<std::string, std::vector<const Render*>> groups;
    for(const auto& render : renders) groups[Scale(render.scale)].push_back(&render);
    size_t complete = 0;
    for(const auto& group : groups) for(size_t offset = 0; offset < group.second.size(); offset += BatchSize) {
        Cancelled(cancel);
        std::vector<std::string> ids;
        const auto count = std::min(BatchSize, group.second.size() - offset);
        for(size_t i = 0; i < count; ++i) ids.push_back(group.second[offset + i]->id);
        auto url = "https://api.figma.com/v1/images/" + Encode(design.location.fileKey) + "?ids=" + Encode(Join(ids, ",")) +
            "&format=png&use_absolute_bounds=true&contents_only=true&scale=" + group.first;
        if(!version.empty()) url += "&version=" + Encode(version);
        Report(progress, "Requesting artwork renders from Figma...");
        const auto response = Parse(Request({url, token, MaxJsonBytes}, cancel, transport).body);
        if(!response.contains("images") || !response["images"].is_object() || (response.contains("err") && !response["err"].is_null() && response["err"] != ""))
            throw std::runtime_error("Figma could not render the selected artwork. Try again later or use the local export plugin.");
        for(size_t i = 0; i < count; ++i) {
            Cancelled(cancel);
            const auto* render = group.second[offset + i];
            const auto image = response["images"].find(render->id);
            if(image == response["images"].end() || !image->is_string() || image->get_ref<const std::string&>().empty())
                throw std::runtime_error("Figma could not render a selected artwork layer. Make hidden artwork visible in Figma, or exclude hidden layers and review again.");
            const auto imageUrl = image->get<std::string>();
            if(imageUrl.size() > 16384 || Lower(imageUrl.substr(0, 8)) != "https://") throw std::runtime_error("Figma returned an invalid artwork download URL.");
            Report(progress, "Downloading Figma artwork " + std::to_string(complete + 1) + " of " + std::to_string(renders.size()) + "...");
            const auto replaced = cachedSizes.find(render->id);
            const auto oldBytes = replaced == cachedSizes.end() ? size_t(0) : replaced->second.first;
            const auto oldPixels = replaced == cachedSizes.end() ? uint64_t(0) : replaced->second.second;
            const auto png = Request({imageUrl, {}, MaxImageBytes - (downloaded - oldBytes)}, cancel, transport).body;
            const auto imagePixels = PngPixels(png);
            if(imagePixels > MaxTotalPixels - (pixels - oldPixels)) throw std::runtime_error("Figma artwork exceeds the total decoded image limit. Reload the design and select fewer frames.");
            pixels = pixels - oldPixels + imagePixels;
            downloaded = downloaded - oldBytes + png.size();
            assets[render->id] = {{"name", "Figma artwork.png"}, {"mimeType", "image/png"},
                {"data", base64_encode(reinterpret_cast<const unsigned char*>(png.data()), static_cast<unsigned int>(png.size()))},
                {"bounds", Member(*render->node, "absoluteBoundingBox")}, {"scale", render->scale}, {"reason", render->reason}};
            if(render->scale + 0.0000001 < options.scale)
                AddWarning(result, "Some Figma artwork was reduced to the API's scale or image dimension limit; larger import sizes may appear softer.");
            ++complete;
        }
    }
    Report(progress, "Figma artwork downloaded. Preparing editable widgets...");
    return result;
}
}
