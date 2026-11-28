#include "ObjectIcons.h"
#include <Serialization/ObjectSerializer.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace ObjectIcons {
enum class Shape : uint8_t { Rect, Line, Circle, Arc, Triangle };
enum class Tone : uint8_t { None, Primary, Accent, Blue, Muted };
struct Primitive { Shape shape; Tone stroke, fill; float width; float p[6]; };
namespace {
#include "../Assets/ObjectIcons/ObjectIconData.inc"
struct ActionBinding { const char* action; uint16_t index; };
struct ActionPrefix { const char* prefix; uint16_t index; };
#include "../Assets/ActionIcons/ActionIconData.inc"
const json* Member(const json* value, const char* key) {
    if (!value || !value->is_object()) return nullptr;
    const auto found = value->find(key);
    return found == value->end() ? nullptr : &*found;
}
lv_color_t Color(Tone tone) {
    const uint32_t colors[] = {0, 0xd6e5f5, 0x45d6b3, 0x65afff, 0x8fa8c4};
    return lv_color_hex(colors[static_cast<unsigned>(tone)]);
}
void Paint(lv_event_t* event) {
    auto* object = lv_event_get_current_target_obj(event);
    const auto* icon = static_cast<const Icon*>(lv_event_get_user_data(event));
    auto* layer = lv_event_get_layer(event);
    lv_area_t bounds{}; lv_obj_get_coords(object, &bounds);
    const float size = static_cast<float>((std::min)(lv_obj_get_width(object), lv_obj_get_height(object)));
    const float scale = size / 32.0f;
    const float left = bounds.x1 + (lv_obj_get_width(object) - size) / 2.0f;
    const float top = bounds.y1 + (lv_obj_get_height(object) - size) / 2.0f;
    auto x = [&](float value) { return static_cast<int32_t>(std::lround(left + value * scale)); };
    auto y = [&](float value) { return static_cast<int32_t>(std::lround(top + value * scale)); };
    auto length = [&](float value) { return static_cast<int32_t>(std::lround(value * scale)); };
    for (uint16_t index = 0; index < icon->commandCount; ++index) {
        const auto& command = icon->commands[index]; const auto* p = command.p;
        const auto width = (std::max)(1, length(command.width));
        if (command.shape == Shape::Rect || command.shape == Shape::Circle) {
            const bool circle = command.shape == Shape::Circle;
            const float inset = command.stroke == Tone::None ? 0.0f : command.width / 2.0f;
            const float rx = circle ? p[0] - p[2] : p[0];
            const float ry = circle ? p[1] - p[2] : p[1];
            const float rw = circle ? 2 * p[2] : p[2];
            const float rh = circle ? 2 * p[2] : p[3];
            lv_area_t area{x(rx-inset), y(ry-inset), x(rx+rw+inset)-1, y(ry+rh+inset)-1};
            lv_draw_rect_dsc_t draw; lv_draw_rect_dsc_init(&draw);
            draw.bg_opa = command.fill == Tone::None ? LV_OPA_TRANSP : LV_OPA_COVER;
            draw.bg_color = Color(command.fill);
            draw.border_opa = command.stroke == Tone::None ? LV_OPA_TRANSP : LV_OPA_COVER;
            draw.border_color = Color(command.stroke); draw.border_width = command.stroke == Tone::None ? 0 : width;
            draw.radius = circle ? LV_RADIUS_CIRCLE : length(p[4]+inset);
            lv_draw_rect(layer, &draw, &area);
        } else if (command.shape == Shape::Line) {
            lv_draw_line_dsc_t draw; lv_draw_line_dsc_init(&draw);
            draw.p1.x=x(p[0]); draw.p1.y=y(p[1]); draw.p2.x=x(p[2]); draw.p2.y=y(p[3]);
            draw.color=Color(command.stroke); draw.width=width; draw.round_start=draw.round_end=true;
            lv_draw_line(layer, &draw);
        } else if (command.shape == Shape::Arc) {
            lv_draw_arc_dsc_t draw; lv_draw_arc_dsc_init(&draw);
            draw.center.x=x(p[0]); draw.center.y=y(p[1]); draw.radius=static_cast<uint16_t>(length(p[2]+command.width/2));
            draw.start_angle=p[3]; draw.end_angle=p[4]; draw.color=Color(command.stroke); draw.width=width; draw.rounded=true;
            lv_draw_arc(layer, &draw);
        } else if (command.shape == Shape::Triangle) {
            lv_draw_triangle_dsc_t draw; lv_draw_triangle_dsc_init(&draw);
            for (unsigned point=0; point<3; ++point) { draw.p[point].x=x(p[2*point]); draw.p[point].y=y(p[2*point+1]); }
            draw.color=Color(command.fill); lv_draw_triangle(layer, &draw);
        }
    }
}
}
const Icon* Widget(int id) { return id >= 0 && id < 40 ? &catalog[id] : nullptr; }
const Icon* Preset(int id) { return id >= 0 && id < 49 ? &catalog[40+id] : nullptr; }
const Icon* At(size_t index) { return index < Count() ? &catalog[index] : nullptr; }
size_t Count() { return sizeof(catalog) / sizeof(catalog[0]); }
size_t ActionCount() { return sizeof(actionCatalog) / sizeof(actionCatalog[0]); }
const Icon* ActionAt(size_t index) { return index < ActionCount() ? &actionCatalog[index] : nullptr; }
const Icon* Action(const char* action) {
    if (!action || !*action) return nullptr;
    for (const auto& binding : actionBindings)
        if (std::strcmp(binding.action, action) == 0) return &actionCatalog[binding.index];
    for (const auto& binding : actionPrefixes) {
        const auto length = std::strlen(binding.prefix);
        if (std::strncmp(binding.prefix, action, length) == 0 && action[length]) return &actionCatalog[binding.index];
    }
    for (const auto& icon : actionCatalog)
        if (std::strcmp(icon.key, action) == 0) return &icon;
    return nullptr;
}
const Icon* Find(const char* key) {
    if (!key || !*key) return nullptr;
    for (const auto& icon : catalog)
        if (std::strcmp(icon.key, key) == 0) return &icon;
    return nullptr;
}
const Icon* ForObject(lv_obj_t* object) {
    if (!object) return Widget(8);
    const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
    const auto* record = data ? &data->objectJson : nullptr;
    const auto* saved = Member(Member(Member(record, "base"), "design"), "paletteIcon");
    if (saved && saved->is_string()) {
        const auto& key = saved->get_ref<const std::string&>();
        const auto* icon = Find(key.c_str());
        // A malformed string with an embedded NUL must not match a valid prefix.
        if (icon && key == icon->key) return icon;
    }
    // TypeName checks specific code/custom wrappers before their native base
    // classes, including QR/barcode canvases and the Advanced Keyboard.
    const auto* type = Serialization::ObjectSerializer::TypeName(object);
    if (type && std::strcmp(type, "scale") == 0) {
        const auto* variant = Member(Member(record, "scale"), "variant");
        return Widget(variant && variant->is_string() && variant->get_ref<const std::string&>() == "gauge" ? 10 : 17);
    }
    struct Kind { const char* type; uint8_t id; };
    static constexpr Kind kinds[] = {
        {"arc",0}, {"bar",1}, {"button",2}, {"buttonMatrix",3},
        {"calendar",4}, {"canvas",5}, {"checkbox",6}, {"chart",7},
        {"object",8}, {"dropdown",9}, {"image",11}, {"imagebutton",12},
        {"keyboard",13}, {"label",14}, {"led",15}, {"line",16},
        {"messagebox",18}, {"page",19}, {"spinner",20}, {"roller",21}, {"slider",22},
        {"spinbox",23}, {"switch",24}, {"table",25}, {"tabview",26},
        {"textarea",27}, {"tileview",28}, {"window",29}, {"textBox",30},
        {"layerView",31}, {"barcode",32}, {"qrcode",33}, {"advancedKeyboard",34},
        {"radioGroup",35}, {"checkboxButton",36}, {"labeledCheckbox",37}, {"datePicker",38}, {"menu",39}
    };
    if (type) for (const auto& kind : kinds)
        if (std::strcmp(type, kind.type) == 0) return Widget(kind.id);
    return Widget(8); // Screens and otherwise unclassified objects use Container.
}
lv_obj_t* Create(lv_obj_t* parent, const Icon* icon, int size) {
    if (!icon || size <= 0) return nullptr;
    auto* object=lv_obj_create(parent); lv_obj_remove_style_all(object);
    lv_obj_set_size(object,size,size); lv_obj_set_style_pad_all(object,0,LV_PART_MAIN);
    lv_obj_set_style_border_width(object,0,LV_PART_MAIN); lv_obj_set_style_bg_opa(object,LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_clickable(object, false);
    lv_obj_set_click_focusable(object, false);
    lv_obj_set_scrollable(object, false);
    lv_obj_add_event_cb(object,Paint,LV_EVENT_DRAW_MAIN,const_cast<Icon*>(icon));
    return object;
}
const Icon* Get(lv_obj_t* object) {
    if (!object) return nullptr;
    for (uint32_t index=0; index<lv_obj_get_event_count(object); ++index) {
        auto* event=lv_obj_get_event_dsc(object,index);
        if (lv_event_dsc_get_cb(event)==Paint) return static_cast<const Icon*>(lv_event_dsc_get_user_data(event));
    }
    return nullptr;
}
void Set(lv_obj_t* iconObject, const Icon* icon) {
    if (!iconObject || !icon) return;
    const auto* previous = Get(iconObject);
    if (!previous || previous == icon) return;
    lv_obj_remove_event_cb(iconObject, Paint);
    lv_obj_add_event_cb(iconObject, Paint, LV_EVENT_DRAW_MAIN, const_cast<Icon*>(icon));
    lv_obj_invalidate(iconObject);
}
}
