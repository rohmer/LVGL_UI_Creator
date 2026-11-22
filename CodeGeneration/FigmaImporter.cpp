#include "FigmaImporter.h"
#include <Serialization/ImageAsset.h>
#include <Serialization/LVFont.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace FigmaImport {
namespace {
constexpr size_t MaxNodes = 50000, MaxDepth = 64, MaxWarnings = 200;
constexpr size_t MaxEncodedAssets = 64 * 1024 * 1024, MaxImagePixels = 64 * 1024 * 1024;
struct Bounds { double x = 0, y = 0, width = 0, height = 0; };
const json& Empty() { static const json empty = json::object(); return empty; }
const json& Member(const json& value, const char* key) {
    const auto found = value.find(key);
    return found == value.end() ? Empty() : *found;
}
std::string String(const json& node, const char* key, const std::string& fallback = {}) {
    const auto found = node.find(key);
    if(found == node.end()) return fallback;
    if(!found->is_string()) throw std::invalid_argument(std::string("Figma ") + key + " must be text");
    const auto value = found->get<std::string>();
    if(value.find('\0') != std::string::npos) throw std::invalid_argument("Figma text contains a NUL character");
    return value;
}
double Number(const json& value, const char* key, double fallback = 0) {
    const auto found = value.find(key);
    if(found == value.end()) return fallback;
    if(!found->is_number()) throw std::invalid_argument(std::string("Figma ") + key + " must be a number");
    const double number = found->get<double>();
    if(!std::isfinite(number) || std::abs(number) > 1.0e9) throw std::invalid_argument(std::string("Invalid Figma ") + key);
    return number;
}
bool Boolean(const json& value, const char* key, bool fallback) {
    const auto found = value.find(key);
    if(found == value.end()) return fallback;
    if(!found->is_boolean()) throw std::invalid_argument(std::string("Figma ") + key + " must be true or false");
    return found->get<bool>();
}
std::string Name(const json& node) {
    auto name = String(node, "name", String(node, "type", "Figma layer"));
    for(auto& ch : name) if(ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
    if(name.empty()) name = "Figma layer";
    if(name.size() > 120) {
        size_t end = 120;
        while(end && (static_cast<unsigned char>(name[end]) & 0xc0) == 0x80) --end;
        name.resize(end);
    }
    return name;
}
Bounds ReadBounds(const json& value) {
    if(!value.is_object()) throw std::invalid_argument("A Figma layer has no absolute bounding box");
    for(const auto* key : {"x", "y", "width", "height"})
        if(!value.contains(key)) throw std::invalid_argument("Incomplete Figma bounding box");
    Bounds result{Number(value,"x"), Number(value,"y"), Number(value,"width"), Number(value,"height")};
    if(result.width < 0 || result.height < 0) throw std::invalid_argument("Negative Figma layer dimensions");
    return result;
}
Bounds Box(const json& node) { return ReadBounds(Member(node, "absoluteBoundingBox")); }
int Dimension(double value) {
    if(!std::isfinite(value) || value > 32767.0) throw std::invalid_argument("Imported dimensions must be at most 32767 pixels; reduce the import scale");
    return std::max(1, static_cast<int>(std::lround(value)));
}
int Coordinate(double value) {
    if(!std::isfinite(value) || std::abs(value) > 1000000) throw std::invalid_argument("Figma layer coordinates are too large");
    return static_cast<int>(std::lround(value));
}
int Alpha(double value) { return static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 255)); }
int Color(const json& color) {
    if(!color.is_object()) throw std::invalid_argument("Invalid Figma color");
    auto byte = [&](const char* key) { return Alpha(Number(color, key)); };
    return (byte("r") << 16) | (byte("g") << 8) | byte("b");
}
bool Structural(const std::string& type) { return type == "DOCUMENT" || type == "CANVAS" || type == "SECTION"; }
struct Tree {
    std::vector<const json*> roots, candidates;
    std::map<std::string,const json*> nodes;
    std::map<std::string,bool> hidden;
    explicit Tree(const json& source) {
        if(!source.is_object()) throw std::invalid_argument("Choose a Figma JSON export or UI Creator Figma bundle");
        if(source.contains("uicreatorFigmaVersion") && source["uicreatorFigmaVersion"] != 1)
            throw std::invalid_argument("Unsupported UI Creator Figma bundle version");
        if(source.contains("document")) {
            if(!source["document"].is_object()) throw std::invalid_argument("Figma document must be an object");
            roots.push_back(&source["document"]);
        } else if(source.contains("nodes")) {
            if(!source["nodes"].is_object()) throw std::invalid_argument("Figma nodes must be an object");
            for(const auto& item : source["nodes"].items()) {
                if(item.value().is_null()) continue;
                if(!item.value().is_object() || !item.value().contains("document") || !item.value()["document"].is_object())
                    throw std::invalid_argument("Invalid Figma nodes response");
                roots.push_back(&item.value()["document"]);
            }
        } else if(source.contains("type")) roots.push_back(&source);
        else throw std::invalid_argument("This file contains no Figma document or nodes");
        // /files/:key/nodes can contain both a parent and one of its children.
        // Keep the encompassing subtree once while still rejecting duplicate
        // IDs inside that subtree during ordinary validation.
        if(roots.size() > 1) {
            std::set<std::string> rootIds, nestedRoots;
            for(const auto* root : roots) rootIds.insert(String(*root,"id"));
            size_t visited = 0;
            for(const auto* root : roots) MarkNestedRoots(*root,rootIds,nestedRoots,0,visited);
            roots.erase(std::remove_if(roots.begin(),roots.end(),[&](const json* root){return nestedRoots.count(String(*root,"id"));}),roots.end());
        }
        for(const auto* root : roots) Validate(*root, 0, false);
        for(const auto* root : roots) Collect(*root);
        if(candidates.empty()) throw std::invalid_argument("The Figma design contains no importable frames or layers");
    }
    static void MarkNestedRoots(const json& node,const std::set<std::string>& rootIds,std::set<std::string>& nested,size_t depth,size_t& visited) {
        if(depth > MaxDepth || ++visited > MaxNodes*2) throw std::invalid_argument("Figma nodes response is too large or deeply nested");
        if(!node.is_object()) throw std::invalid_argument("Figma layers must be objects");
        if(depth && rootIds.count(String(node,"id"))) nested.insert(String(node,"id"));
        if(node.contains("children")) {
            if(!node["children"].is_array()) throw std::invalid_argument("Figma children must be an array");
            for(const auto& child : node["children"]) MarkNestedRoots(child,rootIds,nested,depth+1,visited);
        }
    }
    void Validate(const json& node, size_t depth, bool inheritedHidden) {
        if(depth > MaxDepth) throw std::invalid_argument("Figma layer nesting exceeds 64 levels");
        if(!node.is_object()) throw std::invalid_argument("Figma layers must be objects");
        const auto id = String(node,"id"), type = String(node,"type");
        if(id.empty() || id.size() > 256 || type.empty()) throw std::invalid_argument("Every Figma layer needs an ID and type");
        if(!nodes.emplace(id, &node).second) throw std::invalid_argument("Duplicate Figma layer ID: " + id);
        if(nodes.size() > MaxNodes) throw std::invalid_argument("Figma import supports at most 50000 layers");
        Name(node);
        const bool isHidden = inheritedHidden || !Boolean(node,"visible",true);
        hidden[id] = isHidden;
        if(!Structural(type) && (!isHidden || (node.contains("absoluteBoundingBox") && !node["absoluteBoundingBox"].is_null()))) Box(node);
        for(const auto* key : {"fills","strokes","effects"})
            if(node.contains(key) && !node[key].is_array()) throw std::invalid_argument(std::string("Figma ") + key + " must be an array");
        if(node.contains("style") && !node["style"].is_object()) throw std::invalid_argument("Figma text style must be an object");
        if(node.contains("children")) {
            if(!node["children"].is_array()) throw std::invalid_argument("Figma children must be an array");
            for(const auto& child : node["children"]) Validate(child,depth+1,isHidden);
        }
    }
    void Collect(const json& node) {
        if(Structural(String(node,"type"))) {
            if(node.contains("children")) for(const auto& child : node["children"]) Collect(child);
        } else if(String(node,"type") != "SLICE") candidates.push_back(&node);
    }
};
struct Converter {
    const json& source;
    const Tree& tree;
    const Options& options;
    Result result;
    std::set<std::string> warned;
    std::map<std::string,std::string> imageIds;
    size_t encodedBytes = 0, imagePixels = 0;
    Converter(const json& input,const Tree& document,const Options& settings) : source(input),tree(document),options(settings) {
        result.project = {{"formatVersion",2},{"lvglVersion","9.6.0"},{"screens",json::array()},
            {"variables",json::array()},{"fonts",json::array()},{"images",json::array()},{"tries",json::array()},
            {"activeScreen",0},{"editorScreen",0}};
        if(source.contains("uicreatorAssets") && !source["uicreatorAssets"].is_object())
            throw std::invalid_argument("Figma bundled assets must be an object");
        if(source.contains("uicreatorExportWarnings")) {
            if(!source["uicreatorExportWarnings"].is_array()) throw std::invalid_argument("Invalid Figma export warnings");
            for(const auto& warning : source["uicreatorExportWarnings"]) {
                if(!warning.is_string()) throw std::invalid_argument("Invalid Figma export warning");
                Warn(warning.get<std::string>());
            }
        }
    }
    json Font(double size) {
        std::vector<std::pair<int,const lv_font_t*>> fonts;
#define FIGMA_FONT(size) fonts.emplace_back(size,&lv_font_montserrat_##size)
#if LV_FONT_MONTSERRAT_8
        FIGMA_FONT(8);
#endif
#if LV_FONT_MONTSERRAT_10
        FIGMA_FONT(10);
#endif
#if LV_FONT_MONTSERRAT_12
        FIGMA_FONT(12);
#endif
#if LV_FONT_MONTSERRAT_14
        FIGMA_FONT(14);
#endif
#if LV_FONT_MONTSERRAT_16
        FIGMA_FONT(16);
#endif
#if LV_FONT_MONTSERRAT_18
        FIGMA_FONT(18);
#endif
#if LV_FONT_MONTSERRAT_20
        FIGMA_FONT(20);
#endif
#if LV_FONT_MONTSERRAT_22
        FIGMA_FONT(22);
#endif
#if LV_FONT_MONTSERRAT_24
        FIGMA_FONT(24);
#endif
#if LV_FONT_MONTSERRAT_26
        FIGMA_FONT(26);
#endif
#if LV_FONT_MONTSERRAT_28
        FIGMA_FONT(28);
#endif
#if LV_FONT_MONTSERRAT_30
        FIGMA_FONT(30);
#endif
#if LV_FONT_MONTSERRAT_32
        FIGMA_FONT(32);
#endif
#if LV_FONT_MONTSERRAT_34
        FIGMA_FONT(34);
#endif
#if LV_FONT_MONTSERRAT_36
        FIGMA_FONT(36);
#endif
#if LV_FONT_MONTSERRAT_38
        FIGMA_FONT(38);
#endif
#if LV_FONT_MONTSERRAT_40
        FIGMA_FONT(40);
#endif
#if LV_FONT_MONTSERRAT_42
        FIGMA_FONT(42);
#endif
#if LV_FONT_MONTSERRAT_44
        FIGMA_FONT(44);
#endif
#if LV_FONT_MONTSERRAT_46
        FIGMA_FONT(46);
#endif
#if LV_FONT_MONTSERRAT_48
        FIGMA_FONT(48);
#endif
#undef FIGMA_FONT
        if(size <= 0) throw std::invalid_argument("Figma text size must be positive");
        if(fonts.empty()) return "default";
        const auto font = std::min_element(fonts.begin(),fonts.end(),[&](const auto& a,const auto& b){return std::abs(a.first-size) < std::abs(b.first-size);});
        if(font->second == LV_FONT_DEFAULT) return "default";
        // Serialize a real LVGL bitmap font; native styles do not resolve
        // arbitrary font-name strings. Shared project entries deduplicate it.
        const auto id = "figma-montserrat-" + std::to_string(font->first);
        auto& catalog = result.project["fonts"];
        if(std::none_of(catalog.begin(),catalog.end(),[&](const json& item){return item["id"] == id;}))
            catalog.push_back({{"id",id},{"name","Figma Montserrat "+std::to_string(font->first)},{"font",Serialization::LVFont::ToJSON(*font->second)}});
        return {{"projectFont",id}};
    }
    void Warn(const std::string& text) {
        if(!warned.insert(text).second) return;
        if(result.warnings.size() < MaxWarnings) result.warnings.push_back(text);
        else if(result.warnings.size() == MaxWarnings) result.warnings.push_back("Additional import warnings were omitted.");
    }
    const json* Asset(const json& node) const {
        const auto assets = source.find("uicreatorAssets");
        if(assets == source.end()) return nullptr;
        const auto found = assets->find(String(node,"id"));
        return found == assets->end() ? nullptr : &*found;
    }
    std::string Image(const json& node,const json& asset) {
        const auto id = String(node,"id");
        const auto previous = imageIds.find(id);
        if(previous != imageIds.end()) return previous->second;
        if(!asset.is_object()) throw std::invalid_argument("Invalid Figma bundled image: " + id);
        const auto data = String(asset,"data");
        if(data.empty() || data.size() % 4 != 0 || data.size() > MaxEncodedAssets - encodedBytes)
            throw std::invalid_argument("Figma bundled images are empty, invalid, or exceed 64 MiB");
        size_t padding = 0;
        for(size_t i = 0; i < data.size(); ++i) {
            const unsigned char ch = static_cast<unsigned char>(data[i]);
            if(ch == '=') { ++padding; if(i + 2 < data.size() || padding > 2) throw std::invalid_argument("Invalid Figma image base64"); }
            else if(padding || !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '+' || ch == '/'))
                throw std::invalid_argument("Invalid Figma image base64");
        }
        const auto mime = String(asset,"mimeType","image/png");
        if(mime != "image/png" && mime != "image/jpeg") throw std::invalid_argument("Figma assets must be PNG or JPEG images");
        const auto extension = mime == "image/png" ? ".png" : ".jpg";
        const auto label = "Figma image " + std::to_string(imageIds.size()+1);
        json embedded = {{"name",label+extension},{"sourceData",data}};
        const auto decoded = Serialization::ImageAsset::Decode(embedded);
        // Match the editor's restored asset record so project snapshots keep a
        // shared catalog reference instead of duplicating the encoded pixels.
        embedded["width"] = decoded.width;
        embedded["height"] = decoded.height;
        embedded["frames"] = decoded.delays.size();
        const size_t pixels = static_cast<size_t>(decoded.width) * decoded.height;
        if(pixels > MaxImagePixels - imagePixels || imageIds.size() >= 1024)
            throw std::invalid_argument("Figma images exceed the import pixel or image count limit");
        encodedBytes += data.size(); imagePixels += pixels;
        const auto imageId = "figma-image-" + std::to_string(imageIds.size()+1);
        imageIds[id] = imageId;
        result.project["images"].push_back({{"id",imageId},{"name",label},{"image",embedded}});
        return imageId;
    }
    const json* Paint(const json& node,const char* key) {
        const auto found = node.find(key);
        if(found == node.end()) return nullptr;
        const json* chosen = nullptr;
        size_t visible = 0;
        for(const auto& paint : *found) {
            if(!paint.is_object()) throw std::invalid_argument("Figma paints must be objects");
            if(Boolean(paint,"visible",true)) { chosen = &paint; ++visible; }
        }
        if(visible > 1) Warn("Multiple paint layers are approximated by the top visible paint; use the Figma plugin for raster fidelity.");
        return chosen;
    }
    void Fill(json& values,const json& node,bool text) {
        const auto* paint = Paint(node,"fills");
        if(!paint) return;
        const auto type = String(*paint,"type");
        const json* color = nullptr;
        double opacity = Number(*paint,"opacity",1);
        if(type == "SOLID") color = &Member(*paint,"color");
        else if(type.find("GRADIENT_") == 0 && paint->contains("gradientStops") && (*paint)["gradientStops"].is_array() && !(*paint)["gradientStops"].empty()) {
            color = &Member((*paint)["gradientStops"].front(),"color");
            Warn("Gradients without bundled renders use their first color stop. Export with the Figma plugin to preserve their appearance.");
        } else {
            Warn("Image or unsupported fills without bundled renders could not be reproduced. Export with the Figma plugin to embed them.");
            return;
        }
        opacity *= Number(*color,"a",1);
        values[text ? "text_color" : "bg_color"] = Color(*color);
        values[text ? "text_opa" : "bg_opa"] = Alpha(opacity);
    }
    json Styles(const json& node,const Bounds& bounds) {
        const auto type = String(node,"type");
        const bool text = type == "TEXT", line = type == "LINE";
        json values = {{"bg_opa",0},{"border_width",0},{"radius",0},{"shadow_width",0},
            {"pad_left",0},{"pad_right",0},{"pad_top",0},{"pad_bottom",0},{"pad_row",0},{"pad_column",0},
            {"layout",0},{"clip_corner",Boolean(node,"clipsContent",false) ? 1 : 0},{"opa",Alpha(Number(node,"opacity",1))}};
        Fill(values,node,text);
        double radius = Number(node,"cornerRadius",0);
        if(node.contains("rectangleCornerRadii")) {
            const auto& radii = node["rectangleCornerRadii"];
            if(!radii.is_array() || radii.size() != 4) throw std::invalid_argument("Invalid Figma corner radii");
            for(const auto& value : radii) if(!value.is_number() || !std::isfinite(value.get<double>()) || value.get<double>() < 0 || value.get<double>() > 1e9)
                throw std::invalid_argument("Invalid Figma corner radius");
            radius = radii[0].get<double>();
            if(std::any_of(radii.begin(),radii.end(),[&](const json& value){return value.get<double>() != radius;}))
                Warn("Different corner radii on one shape use its first radius; the Figma plugin can flatten these shapes.");
        }
        if(radius < 0) throw std::invalid_argument("Negative Figma corner radius");
        if(type == "ELLIPSE") {
            radius = std::min(bounds.width,bounds.height)/2;
            if(std::abs(bounds.width-bounds.height) > 0.01) Warn("Non-circular ellipses without bundled renders are approximated by rounded rectangles.");
        }
        values["radius"] = Dimension(std::min(radius*options.scale,32767.0)) - (radius == 0 ? 1 : 0);
        if(const auto* stroke = Paint(node,"strokes")) {
            if(String(*stroke,"type") == "SOLID") {
                const auto& color = Member(*stroke,"color");
                const int width = Coordinate(Number(node,"strokeWeight",1)*options.scale);
                if(width < 0 || width > 32767) throw std::invalid_argument("Invalid Figma stroke width");
                values[line ? "line_width" : "border_width"] = width;
                values[line ? "line_color" : "border_color"] = Color(color);
                values[line ? "line_opa" : "border_opa"] = Alpha(Number(*stroke,"opacity",1)*Number(color,"a",1));
                if(!line && String(node,"strokeAlign","INSIDE") != "INSIDE") Warn("Centered and outside strokes are imported as inside borders.");
            } else Warn("Non-solid strokes without bundled renders are omitted.");
        }
        if(node.contains("effects")) for(const auto& effect : node["effects"]) {
            if(!effect.is_object()) throw std::invalid_argument("Figma effects must be objects");
            if(Boolean(effect,"visible",true)) { Warn("Effects without bundled renders are omitted. Use the Figma plugin to preserve shadows and blurs."); break; }
        }
        if(std::abs(Number(node,"rotation",0)) > 0.00001) Warn("Rotated layers without bundled renders use their axis-aligned bounding boxes.");
        if(Boolean(node,"isMask",false)) Warn("Masks require bundled renders for exact appearance.");
        if(text) {
            const auto& style = Member(node,"style");
            const auto align = String(style,"textAlignHorizontal","LEFT");
            values["text_align"] = align == "CENTER" ? 2 : align == "RIGHT" ? 3 : 1;
            values["text_font"] = Font(Number(style,"fontSize",14)*options.scale);
            values["text_letter_space"] = Coordinate(Number(style,"letterSpacing",0)*options.scale);
            values["text_line_space"] = 0;
            if(align == "JUSTIFIED") Warn("Justified text is imported as left-aligned text.");
            Warn("Figma fonts use the nearest enabled Montserrat size or the editor's default font. Assign converted project fonts after import to match family, weight, size and glyph coverage.");
            if(node.contains("characterStyleOverrides") && node["characterStyleOverrides"].is_array() && !node["characterStyleOverrides"].empty())
                Warn("Mixed text formatting is imported using the layer's main text style.");
            if(String(style,"textAlignVertical","TOP") != "TOP") Warn("Vertical text alignment is imported as top alignment.");
        }
        return values;
    }
    void Node(const json& node,json& objects,int parent,const Bounds& parentBounds,int parentBorder = 0) {
        const auto id = String(node,"id"), type = String(node,"type");
        if(!options.includeHidden && tree.hidden.at(id)) return;
        if(type == "SLICE") { Warn("Figma slice export guides are not widgets and were skipped."); return; }
        const auto* asset = Asset(node);
        if(tree.hidden.at(id) && (!node.contains("absoluteBoundingBox") || node["absoluteBoundingBox"].is_null()) &&
           (!asset || !asset->is_object() || !asset->contains("bounds"))) {
            Warn("Hidden layers without exported bounding boxes were skipped because Figma provided no geometry.");
            return;
        }
        Bounds bounds = asset && asset->is_object() && asset->contains("bounds") ? ReadBounds((*asset)["bounds"]) : Box(node);
        const int oid = static_cast<int>(objects.size());
        const bool text = type == "TEXT", line = type == "LINE";
        json values;
        if(asset) values = {{"bg_opa",0},{"border_width",0},{"radius",0},{"shadow_width",0},{"pad_left",0},{"pad_right",0},{"pad_top",0},{"pad_bottom",0},{"layout",0},{"opa",255}};
        else values = Styles(node,bounds);
        json base = {{"name",Name(node)},{"coords",{{"x",Coordinate((bounds.x-parentBounds.x)*options.scale)-parentBorder},
            {"y",Coordinate((bounds.y-parentBounds.y)*options.scale)-parentBorder},{"width",Dimension(bounds.width*options.scale)},{"height",Dimension(bounds.height*options.scale)}}},
            {"nativeStyles",json::array({{{"selector",0},{"values",values}}})},{"hidden",!Boolean(node,"visible",true)},
            {"click",false},{"drag",true},{"dragDir",3},{"scroll",{{"enabled",false},{"x",0},{"y",0}}},
            {"flags",{{"scrollable",false},{"overflow_visible",!Boolean(node,"clipsContent",false)}}},
            {"figma",{{"id",id},{"type",type}}}};
        json object = {{"oid",oid},{"par",parent},{"base",std::move(base)}};
        if(asset) {
            const auto imageId = Image(node,*asset);
            object["base"]["imageId"] = imageId;
            object["img"] = {{"asset",{{"projectImage",imageId}}},{"innerAlign",static_cast<int>(LV_IMAGE_ALIGN_STRETCH)},{"antialias",true}};
            Warn("Complex layers with bundled renders are imported as editable image widgets; their internal paths and effects are flattened.");
        } else if(text) {
            object["text"] = String(node,"characters");
            object["longmode"] = 1;
            object["base"]["figma"]["textStyle"] = Member(node,"style");
        } else if(line) {
            object["line"] = {{"points",json::array({json::array({0,0}),json::array({Coordinate(bounds.width*options.scale),Coordinate(bounds.height*options.scale)})})},{"invertY",false}};
            if(bounds.height > 0.01 && !node.contains("relativeTransform")) Warn("Diagonal lines without transform data are drawn from top left to bottom right.");
        } else if(type != "FRAME" && type != "GROUP" && type != "COMPONENT" && type != "COMPONENT_SET" && type != "INSTANCE" && type != "RECTANGLE" && type != "ELLIPSE")
            Warn("Unsupported Figma layer type " + type + " uses a bounding-box widget. Export with the Figma plugin to preserve vectors and complex layers.");
        if(!asset && String(node,"layoutMode","NONE") != "NONE") Warn("Auto layout is imported as its current fixed geometry; positions and sizes remain editable.");
        objects.push_back(std::move(object));
        ++result.objects;
        if(!asset && node.contains("children")) {
            const int border = values.value("border_width",0);
            for(const auto& child : node["children"]) Node(child,objects,oid,bounds,border);
        }
    }
    void Screen(const json& frame) {
        if(!options.includeHidden && tree.hidden.at(String(frame,"id"))) return;
        if(!frame.contains("absoluteBoundingBox") || frame["absoluteBoundingBox"].is_null()) {
            Warn("Hidden frames without exported bounding boxes were skipped because Figma provided no geometry.");
            return;
        }
        const auto bounds = Box(frame);
        if(bounds.width <= 0 || bounds.height <= 0) throw std::invalid_argument("Figma screens must have positive dimensions");
        json objects = json::array();
        Node(frame,objects,-1,bounds);
        if(objects.empty()) return;
        const int width = Dimension(bounds.width*options.scale), height = Dimension(bounds.height*options.scale);
        json canvas = {{"width",width},{"height",height},{"base",{{"name","Figma canvas"},{"nativeStyles",json::array({{{"selector",0},{"values",{{"bg_color",0xffffff},{"bg_opa",255},{"border_width",0},{"pad_left",0},{"pad_top",0},{"pad_right",0},{"pad_bottom",0}}}}})},{"scroll",{{"enabled",false}}}}}};
        result.project["screens"].push_back({{"id","figma-screen-"+std::to_string(result.project["screens"].size()+1)},
            {"name",Name(frame)},{"canvas",std::move(canvas)},{"obj",std::move(objects)}});
        if(result.project["screens"].size() > 256) throw std::invalid_argument("Choose at most 256 Figma screens");
    }
};
}
std::vector<Frame> Frames(const json& source) {
    const Tree tree(source);
    std::vector<Frame> frames;
    for(const auto* node : tree.candidates) {
        const auto id = String(*node,"id");
        if(!node->contains("absoluteBoundingBox") || (*node)["absoluteBoundingBox"].is_null()) continue;
        const auto bounds = Box(*node);
        if(bounds.width <= 0 || bounds.height <= 0) continue;
        // Candidate sizes can exceed the device's limit until the user selects
        // an import scale. Keep the source dimensions visible in the chooser.
        frames.push_back({id,Name(*node),Coordinate(bounds.width),Coordinate(bounds.height),tree.hidden.at(id)});
    }
    return frames;
}
Result Convert(const json& source,const Options& options) {
    if(!std::isfinite(options.scale) || options.scale < 0.001 || options.scale > 100)
        throw std::invalid_argument("Figma import scale must be between 0.001 and 100");
    const Tree tree(source);
    Converter convert(source,tree,options);
    if(options.nodeIds.empty()) for(const auto* frame : tree.candidates) convert.Screen(*frame);
    else {
        std::set<std::string> selected;
        for(const auto& id : options.nodeIds) {
            if(!selected.insert(id).second) throw std::invalid_argument("A Figma frame was selected more than once");
            const auto found = tree.nodes.find(id);
            if(found == tree.nodes.end() || Structural(String(*found->second,"type"))) throw std::invalid_argument("The selected Figma frame or layer is missing: " + id);
            convert.Screen(*found->second);
        }
    }
    if(convert.result.project["screens"].empty()) throw std::invalid_argument("No visible Figma frames were selected");
    convert.result.project["canvas"] = convert.result.project["screens"][0]["canvas"];
    convert.result.project["obj"] = convert.result.project["screens"][0]["obj"];
    return std::move(convert.result);
}
}
