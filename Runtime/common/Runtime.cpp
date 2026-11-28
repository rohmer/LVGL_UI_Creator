#include <RuntimeCompat.h>
#include "lvgl_runtime.h"
#include "Validation.h"
#include "../../CodeGeneration/Targets/Target.h"
#if LVGL_VERSION_MAJOR == 8
extern "C" { ui_compat_base* lvgl_runtime_compat_states = nullptr; }
#endif
#include <Serialization/AssetProvider.h>
#include <Serialization/AdvancedKeyboard.h>
#include <Serialization/AdvancedKeyboardMigration.h>
#include <Serialization/Base64.h>
#include <Serialization/CustomWidgets.h>
#include <Serialization/RuntimeFeatures.h>
#if LVSERIAL_USE_XML
#include <Serialization/Xml.h>
#endif
#include <Serialization/ExtendedWidgets.h>
#include <Serialization/Project.h>
#include <Serialization/ProjectFonts.h>
#include <Serialization/ProjectImages.h>
#include <Serialization/ProjectTries.h>
#include <Serialization/ResourceArena.h>
#include <Serialization/InteractionActions.h>
#include <Serialization/ProjectDesign.h>
#include <Serialization/FocusOrder.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <chrono>
#if __has_include(<src/misc/cache/instance/lv_image_cache.h>)
#include <src/misc/cache/instance/lv_image_cache.h>
#include <src/misc/cache/instance/lv_image_header_cache.h>
#elif LVGL_VERSION_MAJOR == 9
#include <src/misc/cache/lv_image_cache.h>
#endif
#ifndef LVGL_RUNTIME_VERSION
#define LVGL_RUNTIME_VERSION "9.6"
#endif
#if LVGL_VERSION_MAJOR != LVGL_RUNTIME_MAJOR || LVGL_VERSION_MINOR != LVGL_RUNTIME_MINOR
#error Selected runtime backend does not match the linked LVGL headers
#endif
using json = nlohmann::json;
namespace
{
// LVGL 8.3's interruption path calls scr_load_internal(), which clears
// scr_to_load before lv_scr_load_anim() dereferences it. Its display structure
// is public; clear the pending animation safely before using the load API.
void CancelLegacyScreenTransition(lv_display_t* display)
{
#if LVGL_VERSION_MAJOR == 8
    if(!display||!display->scr_to_load)return;
    auto* pending=display->scr_to_load;
    display->scr_to_load=nullptr;
    lv_anim_delete(pending,nullptr);
    lv_obj_set_pos(pending,0,0);
    lv_obj_remove_local_style_prop(pending,LV_STYLE_OPA,0);
    if(display->act_scr) {
        lv_anim_delete(display->act_scr,nullptr);
        lv_obj_set_pos(display->act_scr,0,0);
        lv_obj_remove_local_style_prop(display->act_scr,LV_STYLE_OPA,0);
    }
    if(display->prev_scr&&display->prev_scr!=display->act_scr) {
        lv_anim_delete(display->prev_scr,nullptr);
        lv_obj_set_pos(display->prev_scr,0,0);
        lv_obj_remove_local_style_prop(display->prev_scr,LV_STYLE_OPA,0);
    }
    display->prev_scr=nullptr;display->draw_prev_over_act=false;display->del_prev=false;
#else
    (void)display;
#endif
}
void ConvertCoordinates(json& value) {
    if(value.is_object() && value.contains("coordinateKind")) {
        auto kind=value.at("coordinateKind").get<std::string>();int n=value.value("value",0);
        if(kind=="percent")value=LV_PCT(n);else if(kind=="content")value=LV_SIZE_CONTENT;
        else if(kind=="maximum")value=LV_COORD_MAX;else if(kind=="minimum")value=LV_COORD_MIN;
        
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_GRID
        else if(kind=="gridContent")value=LV_GRID_CONTENT;else if(kind=="gridFraction")value=LV_GRID_FR(n);
#endif
        else throw std::invalid_argument("Unknown coordinate token");
        return;
    }
    if(value.is_object() || value.is_array())for(auto& item:value.items())ConvertCoordinates(item.value());
}
void Error(char *buffer, size_t size, const std::string &text)
{
    if (buffer && size)
    {
        auto n = (std::min)(size - 1, text.size());
        std::memcpy(buffer, text.data(), n);
        buffer[n] = 0;
    }
}
bool IsXmlInput(const void* bytes, size_t size)
{
    const auto* input = static_cast<const uint8_t*>(bytes);
    size_t position = size >= 3 && input[0] == 0xef && input[1] == 0xbb && input[2] == 0xbf ? 3 : 0;
    while (position < size && (input[position] == ' ' || input[position] == '\t' ||
           input[position] == '\r' || input[position] == '\n')) ++position;
    return position < size && input[position] == '<';
}
struct FocusSlot{lv_obj_t* object=nullptr;};
struct Screen
{
    lv_obj_t *object = nullptr;
    int32_t width = 0, height = 0;
    std::map<int, lv_obj_t *> objects;
    json definition;
    std::vector<std::unique_ptr<FocusSlot>> focus;
    std::unique_ptr<Serialization::InteractionActions::Context> interactions;
    bool pending_unload = false, loaded_once = false, managed_unload = false;
};
struct Image
{
    json asset;
    std::vector<std::string> bytes;
    std::vector<lv_image_dsc_t> frames;
    std::vector<uint32_t> delays;
    ~Image()
    {
        for (auto &frame : frames)
        {
            lv_image_cache_drop(&frame);
            lv_image_header_cache_drop(&frame);
        }
    }
};
struct Animation
{
    lv_obj_t *object;
    Image *image;
    size_t index = 0;
    lv_timer_t *timer = nullptr;
    int target = 0, state = 0, side = 0;
    lv_style_selector_t selector = 0;
};
struct Variable;
void Deleted(lv_event_t *e)
{
    *static_cast<lv_obj_t **>(lv_event_get_user_data(e)) = nullptr;
}
void Track(lv_obj_t *object, lv_obj_t **slot)
{
    *slot = object;
    lv_obj_add_event_cb(object, Deleted, LV_EVENT_DELETE, slot);
}
void Animate(Animation *a)
{
    if (!a->object)
        return;
    auto *source = &a->image->frames[a->index];
    if (a->target == 0) {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
        lv_image_set_src(a->object, source);
#else
        throw std::invalid_argument("Image widget disabled in this runtime");
#endif
    }
    else if (a->target == 4)
    {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGEBUTTON
        const void *parts[] = {lv_imagebutton_get_src_left(a->object, (lv_imagebutton_state_t)a->state),
                               lv_imagebutton_get_src_middle(a->object, (lv_imagebutton_state_t)a->state),
                               lv_imagebutton_get_src_right(a->object, (lv_imagebutton_state_t)a->state)};
        parts[a->side] = source;
        lv_imagebutton_set_src(a->object, (lv_imagebutton_state_t)a->state, parts[0], parts[1], parts[2]);
#else
        throw std::invalid_argument("Image button disabled in this runtime");
#endif
    }
    else if (a->target == 1)
        lv_obj_set_style_bg_image_src(a->object, source, a->selector);
    else if (a->target == 2)
        lv_obj_set_style_arc_image_src(a->object, source, a->selector);
    else
        lv_obj_set_style_bitmap_mask_src(a->object, source, a->selector);
}
} // namespace
struct lvgl_runtime : Serialization::AssetProvider
{
    Serialization::ResourceArena resources;
    lv_display_t *display = nullptr;
    lv_obj_t *previous = nullptr;
    std::vector<std::unique_ptr<Screen>> screens;
    std::map<std::string, std::unique_ptr<Image>> images;
    std::map<std::string, const lv_image_dsc_t *> imageIds;
    std::map<std::string, const lv_font_t *> fonts;
    std::vector<std::unique_ptr<Animation>> animations;
    std::vector<std::unique_ptr<Variable>> variables;
    Serialization::ProjectTries::CatalogPtr tries;
    lv_group_t *input_group = nullptr;
    lv_timer_t *unload_timer = nullptr;
    std::vector<size_t> back;
    size_t input_bytes = 0, navigation_target = SIZE_MAX;
    uint64_t load_us = 0, last_screen_load_us = 0, screen_loads = 0, screen_unloads = 0;
    bool lazy = false, unload_inactive = false, initialized = false;
    std::string error;
    bool updating = false;
    const void *Resolve(const json &asset) override;
    Image &ResourceImage(const json &asset)
    {
        auto *descriptor = Resolve(asset);
        for (auto &item : images)
            if (&item.second->frames[0] == descriptor)
                return *item.second;
        throw std::invalid_argument("Image resource not found");
    }
    json Save(const void *source) override
    {
        for (const auto &item : images)
            for (const auto &frame : item.second->frames)
                if (source == &frame)
                    return {{"asset", item.second->asset}};
        return nullptr;
    }
    ~lvgl_runtime();
};
namespace
{
struct VariableBindingState
{
    Variable* variable = nullptr;
    Serialization::VariableBinding definition;
    lv_obj_t* object = nullptr;
    std::string property, type;
    bool readable = false;
};
struct Variable
{
    lvgl_runtime *owner;
    Serialization::VariableData definition;
    json current;
    std::vector<json> history;
    size_t history_start = 0;
    std::vector<std::unique_ptr<VariableBindingState>> bindings;
};
struct DisplayScope
{
    lv_display_t *previous;
    explicit DisplayScope(lv_display_t *value) : previous(lv_display_get_default())
    {
        lv_display_set_default(value);
    }
    ~DisplayScope()
    {
        lv_display_set_default(previous);
    }
};
std::string Property(std::string path)
{
    auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
void ValidateValue(const Variable &variable, const json &value)
{
    if (value.is_string() && value.get_ref<const std::string &>().find('\0') != std::string::npos)
        throw std::invalid_argument("Variable strings cannot contain embedded NUL characters");
    Serialization::LVVariable::ValidateValue(variable.definition, value);
}
void Apply(VariableBindingState &v, const json& x)
{
    if (!v.object || v.definition.direction == Serialization::VariableBindingDirection::FromWidget)
        return;
    auto numeric = [&]() {
        if (x.is_boolean())
            return int64_t(x.get<bool>());
        if (x.is_number_float())
        {
            auto d = x.get<double>();
            if (!std::isfinite(d) || d < double(INT32_MIN) || d > double(INT32_MAX))
                throw std::invalid_argument("Binding value outside LVGL coordinate range");
            return int64_t(d);
        }
        return x.get<int64_t>();
    };
    auto number = [&]() {
        auto n = numeric();
        if (n < INT32_MIN || n > INT32_MAX)
            throw std::invalid_argument("Binding value outside LVGL integer range");
        return int32_t(n);
    };
    if ((v.type == "textBox" && (v.property=="text"||v.property=="value")) ||
        (v.type == "layerView" && v.property=="selected")) {
        auto record=Serialization::ObjectSerializer::SerializeObject(v.object);
        record[v.type][v.property]=x;
        Serialization::CustomWidgets::Apply(v.object,record[v.type]);return;
    }
    if(v.property=="text") {
        auto text=x.is_string()?x.get<std::string>():x.dump();
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        if(v.type=="advancedKeyboard") {
            if(text != lv_advanced_keyboard_get_text(v.object))
                lv_advanced_keyboard_set_text(v.object, text.c_str());
            return;
        }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LABEL
        if(v.type=="label"){lv_label_set_text(v.object,text.c_str());return;}
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TEXTAREA
        if(v.type=="textarea"){lv_textarea_set_text(v.object,text.c_str());return;}
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHECKBOX
        if(v.type=="checkbox"){lv_checkbox_set_text(v.object,text.c_str());return;}
#endif
        throw std::invalid_argument("Text binding widget is disabled");
    }
    if(v.property=="checked"){lv_obj_set_state(v.object,LV_STATE_CHECKED,numeric()!=0);return;}
    if(v.property=="hidden"){lv_obj_set_hidden(v.object,numeric()!=0);return;}
    if(v.property=="disabled"){lv_obj_set_state(v.object,LV_STATE_DISABLED,numeric()!=0);return;}
    if(v.property=="x"){lv_obj_set_x(v.object,number());return;}
    if(v.property=="y"){lv_obj_set_y(v.object,number());return;}
    if(v.property=="width"){lv_obj_set_width(v.object,number());return;}
    if(v.property=="height"){lv_obj_set_height(v.object,number());return;}
    if(v.property=="opascale"){lv_obj_set_style_opa(v.object,(lv_opa_t)number(),0);return;}
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LED
    if(v.property=="brightness" && v.type=="led"){lv_led_set_brightness(v.object,(uint8_t)number());return;}
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_DROPDOWN
    if(v.property=="selected" && v.type=="dropdown"){lv_dropdown_set_selected(v.object,number());return;}
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ROLLER
    if(v.property=="selected" && v.type=="roller"){lv_roller_set_selected(v.object,number(),LV_ANIM_OFF);return;}
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ARC
    if(v.type=="arc"){lv_arc_set_value(v.object,number());return;}
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BAR
    if(v.type=="bar"){lv_bar_set_value(v.object,number(),LV_ANIM_OFF);return;}
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SLIDER
    if(v.type=="slider"){lv_slider_set_value(v.object,number(),LV_ANIM_OFF);return;}
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINBOX
    if(v.type=="spinbox"){lv_spinbox_set_value(v.object,number());return;}
#endif
    throw std::invalid_argument("Binding widget disabled in this runtime");
}
void Apply(Variable& variable)
{
    for(auto& binding:variable.bindings)Apply(*binding,variable.current);
}
void Assign(Variable &v, json value, bool history)
{
    ValidateValue(v, value);
    // Check every output before touching any widget, including unloaded screens.
    for(const auto& binding:v.bindings)if(binding->definition.direction!=Serialization::VariableBindingDirection::FromWidget&&
        binding->property!="text"&&binding->property!="checked"&&binding->property!="hidden"&&binding->property!="disabled") {
        const double number=value.is_boolean()?double(value.get<bool>()):value.get<double>();
        if(!std::isfinite(number)||number<double(INT32_MIN)||number>double(INT32_MAX))
            throw std::invalid_argument("Binding value outside LVGL integer range");
    }
    if(v.definition.type==Serialization::VariableType::Float&&Serialization::LVVariable::CodeType(v.definition)=="float")
        value=double(static_cast<float>(value.get<double>()));
    auto previous = std::move(v.current);
    v.current = std::move(value);
    try
    {
        Apply(v);
    }
    catch (...)
    {
        v.current = std::move(previous);
        throw;
    }
    if (history)
    {
        // A one-entry history is always the current value. Retain only its
        // presence marker instead of a second JSON/string allocation.
        if (v.definition.history == 1) v.history_start = 1;
        else if (v.definition.history && v.history.size() == v.definition.history) {
            v.history[v.history_start] = v.current;
            v.history_start = (v.history_start + 1) % v.history.size();
        } else {
            if (v.definition.history && v.history.size() == v.history.capacity())
                v.history.reserve((std::min)(size_t(v.definition.history), (std::max)(size_t(1), v.history.capacity() * 2)));
            v.history.push_back(v.current);
        }
    }
}
json ReadVariable(const VariableBindingState& v) {
    if((v.type=="textBox"&&(v.property=="text"||v.property=="value")) || (v.type=="layerView"&&v.property=="selected"))
        return Serialization::ObjectSerializer::SerializeObject(v.object)[v.type][v.property];
    if(v.property=="checked")return lv_obj_has_state(v.object,LV_STATE_CHECKED);
#if LVSERIAL_USE_ADVANCED_KEYBOARD
    if(v.property=="text" && v.type=="advancedKeyboard")return lv_advanced_keyboard_get_text(v.object);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TEXTAREA
    if(v.property=="text")return lv_textarea_get_text(v.object);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_DROPDOWN
    if(v.property=="selected" && v.type=="dropdown")return lv_dropdown_get_selected(v.object);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ROLLER
    if(v.property=="selected" && v.type=="roller")return lv_roller_get_selected(v.object);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ARC
    if(v.type=="arc")return lv_arc_get_value(v.object);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SLIDER
    if(v.type=="slider")return lv_slider_get_value(v.object);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BAR
    if(v.type=="bar")return lv_bar_get_value(v.object);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SPINBOX
    if(v.type=="spinbox")return lv_spinbox_get_value(v.object);
#endif
    throw std::invalid_argument("Binding widget disabled in this runtime");
}
void Changed(lv_event_t *e)
{
    auto *binding = static_cast<VariableBindingState *>(lv_event_get_user_data(e));
    auto* v=binding->variable;
    if (v->owner->updating || !binding->object)
        return;
    try
    {
        json x=ReadVariable(*binding);
        if (v->definition.type == Serialization::VariableType::Boolean && !x.is_boolean())
            x = x.get<int64_t>() != 0;
        else if (v->definition.type == Serialization::VariableType::Float)
            x = x.is_boolean() ? double(x.get<bool>()) : x.get<double>();
        else if ((v->definition.type == Serialization::VariableType::Integer ||
                  v->definition.type == Serialization::VariableType::DateTime) &&
                 x.is_boolean())
            x = int64_t(x.get<bool>());
        v->owner->updating = true;
        Assign(*v, std::move(x), true);
        v->owner->updating = false;
    }
    catch (const std::exception &error)
    {
        // A rejected interactive value must not leave two-way peers displaying
        // a value that was never committed to the model/history.
        try {v->owner->updating=true;Apply(*v);}catch(...){}
        v->owner->updating = false;
        v->owner->error = error.what();
    }
}
void Bind(VariableBindingState &v)
{
    auto &r = *v.variable->owner;
    auto &d = v.variable->definition;
    const auto& binding=v.definition;
    v.property = Property(binding.property);
    const auto screen = binding.screen;
    if(screen >= r.screens.size())throw std::invalid_argument("Variable binding references a missing screen: " + d.name);
    const auto& records = r.screens[screen]->definition.at("obj");
    auto record = std::find_if(records.begin(),records.end(),[&](const json& item){return item.value("oid",-1)==binding.objectId;});
    if(record==records.end())throw std::invalid_argument("Variable binding references missing widget: " + d.name);
    Serialization::LVVariable::ValidateBinding(d,binding,*record);
    for(const auto& property:Serialization::LVVariable::BindingProperties(*record,d.type))
        if(Property(property.path)==v.property)v.readable=property.readable;
    v.object = lvgl_runtime_object(&r, screen, binding.objectId);
    if (!v.object) {
        v.type="object";
        for(const auto& kind:std::vector<std::pair<const char*,const char*>>{
            {"text","label"},{"textarea","textarea"},{"textBox","textBox"},{"layerView","layerView"},
            {"advancedKeyboard","advancedKeyboard"},{"cb","checkbox"},{"arc","arc"},{"bar","bar"},
            {"slider","slider"},{"spinbox","spinbox"},{"dropdown","dropdown"},{"roller","roller"},{"led","led"}})
            if(record->contains(kind.first)){v.type=kind.second;break;}
    } else v.type = Serialization::ObjectSerializer::TypeName(v.object);
    if (!v.object) return; // Validate unloaded bindings without instantiating their screen.
    auto *target = v.object;
    if (v.type == "textBox" && v.property == "text")
        target = Serialization::CustomWidgets::Part(v.object, "Entry");
    if (v.readable && binding.direction != Serialization::VariableBindingDirection::ToWidget)
        lv_obj_add_event_cb(target, Changed, LV_EVENT_VALUE_CHANGED, &v);
    Track(v.object, &v.object);
}
void ExportValue(const Variable &v, const json &x, lvgl_runtime_value_t *result)
{
    *result = {};
    result->type = (lvgl_runtime_value_type_t)v.definition.type;
    switch (v.definition.type)
    {
    case Serialization::VariableType::String:
        result->string_value = x.get_ref<const std::string &>().c_str();
        break;
    case Serialization::VariableType::Boolean:
        result->boolean_value = x.get<bool>();
        break;
    case Serialization::VariableType::Float:
        result->real_value = x.get<double>();
        break;
    default:
        result->integer_value = x.get<int64_t>();
    }
}
json ImportValue(const lvgl_runtime_value_t &x)
{
    switch (x.type)
    {
    case LVGL_RUNTIME_STRING:
        if (!x.string_value)
            throw std::invalid_argument("Null string value");
        return x.string_value;
    case LVGL_RUNTIME_BOOLEAN:
        return x.boolean_value;
    case LVGL_RUNTIME_FLOAT:
        return x.real_value;
    case LVGL_RUNTIME_INTEGER:
    case LVGL_RUNTIME_DATETIME:
        return x.integer_value;
    default:
        throw std::invalid_argument("Invalid variable type");
    }
}
Variable *Find(lvgl_runtime_t *r, const char *name)
{
    if (r && name)
        for (auto &v : r->variables)
            if (v->definition.name == name)
                return v.get();
    return nullptr;
}
void Start(lvgl_runtime &runtime, lv_obj_t *object, Image &image, int target, int state = 0, int side = 0,
           lv_style_selector_t selector = 0)
{
    if (image.frames.size() < 2)
        return;
    auto a = std::make_unique<Animation>();
    a->object = object;
    a->image = &image;
    a->target = target;
    a->state = state;
    a->side = side;
    a->selector = selector;
    Track(object, &a->object);
    a->timer = lv_timer_create(
        [](lv_timer_t *timer) {
            auto *a = static_cast<Animation *>(lv_timer_get_user_data(timer));
            if (!a->object)
            {
                lv_timer_delete(timer);
                a->timer = nullptr;
                return;
            }
            a->index = (a->index + 1) % a->image->frames.size();
            Animate(a);
            lv_timer_set_period(timer, a->image->delays[a->index]);
        },
        image.delays[0], a.get());
    runtime.animations.push_back(std::move(a));
}
void AnimateTree(lvgl_runtime &runtime, lv_obj_t *object)
{
    auto *data = static_cast<ObjectUserData *>(lv_obj_get_user_data(object));
    auto record = data ? data->objectJson : json::object();
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
    if (record["img"].contains("asset"))
        Start(runtime, object, runtime.ResourceImage(record["img"]["asset"]), 0);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGEBUTTON
    if (lv_obj_get_class(object) == &lv_imagebutton_class)
    {
        const auto &states = record["imagebutton"]["states"];
        for (size_t state = 0; state < states.size(); ++state)
            for (size_t side = 0; side < 3; ++side)
                if (states[state][side].is_object() && states[state][side].contains("asset"))
                    Start(runtime, object, runtime.ResourceImage(states[state][side]["asset"]), 4, (int)state,
                          (int)side);
    }
#endif
    if (record.contains("base"))
        for (const auto &style : record["base"].value("nativeStyles", json::array()))
            for (const auto &entry : style.at("values").items())
                if ((entry.key() == "bg_image_src" || entry.key() == "arc_image_src" ||
                     entry.key() == "bitmap_mask_src") &&
                    entry.value().is_object() && entry.value().contains("asset"))
                    Start(runtime, object, runtime.ResourceImage(entry.value()["asset"]),
                          entry.key() == "bg_image_src"    ? 1
                          : entry.key() == "arc_image_src" ? 2
                                                           : 3,
                          0, 0, style.value("selector", 0u));
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
        AnimateTree(runtime, lv_obj_get_child(object, i));
}
void Validate(const json &document)
{
    if (!document.is_object())
        throw std::invalid_argument("Project must be an object");
    auto version = document.value("formatVersion", 1);
    if (version < 1 || version > 2)
        throw std::invalid_argument("Unsupported project version");
    if (document.value("runtimeTarget", std::string("9.6")) != LVGL_RUNTIME_VERSION)
        throw std::invalid_argument("Project targets a different LVGL runtime backend");
    auto lvgl = document.value("lvglVersion", std::string("9.6.0"));
    if (lvgl != "9.6" && lvgl.rfind("9.6.", 0) != 0)
        throw std::invalid_argument("Project LVGL version is incompatible with backend 9.6");
    const auto screens = document.find("screens");
    if (screens != document.end() && (!screens->is_array() || screens->empty()))
        throw std::invalid_argument("Project needs at least one screen");
    const size_t screenCount = screens == document.end() ? 1 : screens->size();
    if (document.value("activeScreen", size_t(0)) >= screenCount)
        throw std::invalid_argument("Invalid initial screen");
    for (size_t screenIndex = 0; screenIndex < screenCount; ++screenIndex)
    {
        const auto& screen = screens == document.end() ? document : screens->at(screenIndex);
        if (!screen.is_object() || !screen.contains("obj") || !screen["obj"].is_array())
            throw std::invalid_argument("Screen needs an obj array");
        std::map<int, int> parents;
        int next = 0;
        for (const auto &record : screen["obj"])
        {
            if (!record.is_object() || !record.contains("base") || !record["base"].is_object())
                throw std::invalid_argument("Widget needs base properties");
            if (record.contains("hostPath"))
            {
                if (!record["hostPath"].is_array())
                    throw std::invalid_argument("Internal parent path must be an array");
                for (const auto &part : record["hostPath"])
                    if (!part.is_number_integer() || part.get<int64_t>() < 0 ||
                        part.get<uint64_t>() > INT32_MAX)
                        throw std::invalid_argument("Invalid internal parent path index");
            }
            for (const auto *field : {"oid", "par", "hostIndex"})
                if (record.contains(field) &&
                    (!record[field].is_number_integer() ||
                     record[field].get<int64_t>() < (std::string(field) == "oid" ? 0 : -1) ||
                     record[field].get<int64_t>() > INT32_MAX))
                    throw std::invalid_argument("Invalid widget ID or parent index");
            int id = record.value("oid", next++);
            if (id < 0 || !parents.emplace(id, record.value("par", -1)).second)
                throw std::invalid_argument("Invalid or duplicate widget ID");
        }
        // Parent indices are at least -1, so these values can mark completed
        // paths in the existing map. Each widget is traversed once instead of
        // rebuilding a set for every ancestor chain in a deep hierarchy.
        constexpr int visiting = INT32_MIN, complete = INT32_MIN + 1;
        std::vector<int> path;
        for (const auto &p : parents) {
            int parent = p.first;
            while (parent != -1) {
                auto found = parents.find(parent);
                if (found == parents.end() || found->second == visiting)
                    throw std::invalid_argument("Missing parent or widget hierarchy cycle");
                if (found->second == complete) break;
                path.push_back(parent);
                parent = found->second;
                found->second = visiting;
            }
            for (int id : path) parents.at(id) = complete;
            path.clear();
        }
    }
}
uint64_t Microseconds()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
struct GroupScope {
    lv_group_t* previous;
    explicit GroupScope(lv_group_t* group):previous(lv_group_get_default()){lv_group_set_default(group);}
    ~GroupScope(){lv_group_set_default(previous);}
};
void InstallInteractions(lvgl_runtime& runtime,size_t screen)
{
    auto& state=*runtime.screens.at(screen);
    Serialization::InteractionActions::Hooks hooks;
    hooks.enabled=[owner=&runtime]{return !owner->updating;};
    hooks.object=[owner=&runtime](size_t si,int oid)->lv_obj_t* {
        if(!lvgl_runtime_ensure_screen(owner,si))return nullptr;
        return lvgl_runtime_object(owner,si,oid);
    };
    hooks.navigate=[owner=&runtime](size_t si){return lvgl_runtime_show_screen(owner,si);};
    hooks.read=[owner=&runtime](const std::string& name)->json {
        for(const auto& variable:owner->variables)if(variable->definition.name==name) {
            if(variable->definition.type==Serialization::VariableType::Float)return variable->current.get<double>();
            return variable->current;
        }
        throw std::invalid_argument("Action references a missing variable: "+name);
    };
    hooks.write=[owner=&runtime](const std::string& name,const json& value) {
        for(auto& variable:owner->variables)if(variable->definition.name==name) {
            const bool previous=owner->updating;owner->updating=true;
            try {Assign(*variable,value,true);}catch(...){owner->updating=previous;throw;}
            owner->updating=previous;return;
        }
        throw std::invalid_argument("Action references a missing variable: "+name);
    };
    hooks.error=[owner=&runtime](const std::string& message){owner->error=message;};
    state.interactions=std::make_unique<Serialization::InteractionActions::Context>(std::move(hooks));
    for(const auto& widget:state.definition.at("obj")) {
        const int oid=widget.at("oid");auto found=state.objects.find(oid);
        if(found!=state.objects.end())state.interactions->Bind(found->second,screen,oid,widget.value("base",json::object()).value("events",json::array()));
    }
}
void CloseScreenKeyboard(lvgl_runtime& runtime,size_t screen)
{
#if LVSERIAL_USE_ADVANCED_KEYBOARD
    if(screen<runtime.screens.size())for(const auto& item:runtime.screens[screen]->objects)
        if(item.second && Serialization::AdvancedKeyboard::Type(item.second) &&
            Serialization::ReadSection(item.second,"advancedKeyboard").value("screenManaged",false))
            lv_advanced_keyboard_close(item.second);
#else
    (void)runtime;(void)screen;
#endif
}
void ActivateFocus(lvgl_runtime& runtime,size_t screen)
{
    if(!runtime.input_group){CloseScreenKeyboard(runtime,screen);return;}
    auto* focused=lv_group_get_focused(runtime.input_group);
    for(const auto& state:runtime.screens)for(const auto& slot:state->focus)if(slot->object)lv_group_remove_obj(slot->object);
    if(screen>=runtime.screens.size())return;
    auto& state=*runtime.screens[screen];
    for(const auto& slot:state.focus)if(slot->object)lv_group_add_obj(runtime.input_group,slot->object);
    std::vector<lv_obj_t*> authored;for(const auto& pair:state.objects)if(pair.second)authored.push_back(pair.second);
    Serialization::Accessibility::ApplyFocusOrder(runtime.input_group,authored);
    if(focused&&std::any_of(state.focus.begin(),state.focus.end(),[&](const std::unique_ptr<FocusSlot>& slot){return slot->object==focused;}))
        lv_group_focus_obj(focused);
    // Default-group insertion emits focus while a screen is being constructed.
    // Screen keyboards open only on a later user click/focus, not that setup.
    CloseScreenKeyboard(runtime,screen);
}
void EnsureScreen(lvgl_runtime& runtime,size_t screen)
{
    if(screen>=runtime.screens.size())throw std::invalid_argument("Screen index is outside the project");
    auto& state=*runtime.screens[screen];if(state.object)return;
    if(state.loaded_once&&!state.managed_unload)throw std::invalid_argument("This runtime screen was deleted externally");
    const uint64_t started=Microseconds();
    DisplayScope displayScope(runtime.display);GroupScope groupScope(runtime.input_group);
    CancelLegacyScreenTransition(runtime.display);
    auto* previous=lv_screen_active();
    Serialization::ResourceScope resources(runtime.resources);
    Serialization::AssetScope assets(runtime);
    Serialization::ProjectTries::Scope tries(runtime.tries);
    try {
        Track(lv_obj_create(nullptr),&state.object);
        Serialization::Screen::FromJSON(state.definition.value("canvas",state.definition.value("screen",json::object())),state.object);
        lv_obj_set_pos(state.object,0,0);lv_screen_load(state.object);
        auto objects=Serialization::Page::FromJSON(state.definition);
        if(objects.size()!=state.definition["obj"].size())throw std::invalid_argument("Could not instantiate all widgets");
        for(size_t i=0;i<objects.size();++i)Track(objects[i],&state.objects[state.definition["obj"][i].at("oid").get<int>()]);
        for(const auto& widget:state.definition["obj"])if(widget.contains("hostPath")) {
            auto found=state.objects.find(widget.value("par",-1));auto* host=found==state.objects.end()?nullptr:found->second;
            if(!host)throw std::invalid_argument("Invalid internal widget parent");
            for(const auto& part:widget["hostPath"]){host=lv_obj_get_child(host,part.get<int32_t>());if(!host)throw std::invalid_argument("Invalid internal widget path");}
        }
        if(runtime.input_group) {
#if LV_VERSION_CHECK(9,2,0)
            for(auto* node=static_cast<lv_obj_t**>(lv_ll_get_head(&runtime.input_group->obj_ll));node;node=static_cast<lv_obj_t**>(lv_ll_get_next(&runtime.input_group->obj_ll,node)))
#else
            for(auto* node=static_cast<lv_obj_t**>(_lv_ll_get_head(&runtime.input_group->obj_ll));node;node=static_cast<lv_obj_t**>(_lv_ll_get_next(&runtime.input_group->obj_ll,node)))
#endif
            if(lv_obj_get_screen(*node)==state.object){auto slot=std::make_unique<FocusSlot>();Track(*node,&slot->object);state.focus.push_back(std::move(slot));}
        }
        AnimateTree(runtime,state.object);lv_obj_update_layout(state.object);
        CloseScreenKeyboard(runtime,screen);
        if(runtime.initialized) {
            const bool updating=runtime.updating;runtime.updating=true;
            try {for(auto& variable:runtime.variables)for(auto& binding:variable->bindings)if(binding->definition.screen==screen){Bind(*binding);Apply(*binding,variable->current);}}
            catch(...){runtime.updating=updating;throw;}
            runtime.updating=updating;InstallInteractions(runtime,screen);
        }
        if(previous)lv_screen_load(previous);
        state.pending_unload=false;state.loaded_once=true;state.managed_unload=false;runtime.last_screen_load_us=Microseconds()-started;++runtime.screen_loads;
    } catch(...) {
        if(previous)lv_screen_load(previous);
        if(state.object)lv_obj_delete(state.object);
        state.interactions.reset();state.objects.clear();state.focus.clear();throw;
    }
}
json SnapshotObject(lv_obj_t* object)
{
    auto saved=Serialization::ObjectSerializer::SerializeObject(object);
    // Portable/minimal serializers retain authored configuration to avoid
    // linking every style getter. Capture mutable user state explicitly with
    // the same widget feature gates as construction. Never snapshot the
    // Advanced Keyboard's transient overlay geometry or attached text twice.
    if(saved.contains("advancedKeyboard"))return saved;
    auto& base=saved["base"];
    base["hidden"]=lv_obj_is_hidden(object);
    base["checked"]=lv_obj_has_state(object,LV_STATE_CHECKED);
    base["disabled"]=lv_obj_has_state(object,LV_STATE_DISABLED);
    if(base.contains("flags")&&base["flags"].is_object()) {
        auto& flags=base["flags"];
        for(const auto* key:{"hidden","checked","disabled"})if(flags.contains(key))flags[key]=base[key];
    }
#if LV_USE_TEXTAREA
    if(saved.contains("textarea")) {
        auto& state=saved["textarea"];state["text"]=lv_textarea_get_text(object);
        state["cursor"]=lv_textarea_get_cursor_pos(object);
        state["maxLength"]=lv_textarea_get_max_length(object);
        const auto* accepted=lv_textarea_get_accepted_chars(object);
        state["accepted"]=accepted?json(accepted):json(nullptr);
        state["oneLine"]=lv_textarea_get_one_line(object);
        state["password"]=lv_textarea_get_password_mode(object);
    }
#endif
#if LV_USE_SLIDER
    if(saved.contains("slider")) {
        auto& state=saved["slider"];state["min"]=lv_slider_get_min_value(object);state["max"]=lv_slider_get_max_value(object);
        state["value"]=lv_slider_get_value(object);state["leftValue"]=lv_slider_get_left_value(object);
    }
#endif
#if LV_USE_SPINBOX
    if(saved.contains("spinbox"))saved["spinbox"]["value"]=lv_spinbox_get_value(object);
#endif
#if LV_USE_DROPDOWN
    if(saved.contains("dropdown"))saved["dropdown"]["selected"]=lv_dropdown_get_selected(object);
#endif
#if LV_USE_ROLLER
    if(saved.contains("roller"))saved["roller"]["selected"]=lv_roller_get_selected(object);
#endif
#if LV_USE_SWITCH
    if(saved.contains("switch"))saved["switch"]["checked"]=lv_obj_has_state(object,LV_STATE_CHECKED);
#endif
#if LV_USE_CHECKBOX
    if(saved.contains("cb")) {
        saved["cb"]["text"]=lv_checkbox_get_text(object);
        saved["cb"]["checked"]=lv_obj_has_state(object,LV_STATE_CHECKED);
        saved["cb"]["inact"]=lv_obj_has_state(object,LV_STATE_DISABLED);
    }
#endif
#if LV_USE_ARC
    if(saved.contains("arc")) {
        auto& state=saved["arc"];state["value"]=lv_arc_get_value(object);
        state["range"]={{"min",lv_arc_get_min_value(object)},{"max",lv_arc_get_max_value(object)}};
    }
#endif
#if LV_USE_BAR
    if(saved.contains("bar")) {
        auto& state=saved["bar"];state["value"]=lv_bar_get_value(object);state["startValue"]=lv_bar_get_start_value(object);
        state["range"]={{"min",lv_bar_get_min_value(object)},{"max",lv_bar_get_max_value(object)}};
    }
#endif
#if LV_USE_BUTTONMATRIX
    if(saved.contains("btnm"))saved["btnm"]["selected"]=lv_buttonmatrix_get_selected_button(object);
#endif
#if LV_USE_TABVIEW
    if(saved.contains("tabview"))saved["tabview"]["active"]=lv_tabview_get_tab_active(object);
#endif
#if LV_USE_TILEVIEW
    if(saved.contains("tileview")) {
        auto* active=lv_tileview_get_tile_active(object);
        if(active)saved["tileview"]["active"]=lv_obj_get_index(active);
    }
#endif
#if LV_USE_TABLE
    if(saved.contains("table")) {
#if LVGL_VERSION_MAJOR == 8
        uint16_t row,column;
#else
        uint32_t row,column;
#endif
        lv_table_get_selected_cell(object,&row,&column);
        saved["table"]["selectedRow"]=row;saved["table"]["selectedColumn"]=column;
    }
#endif
    return saved;
}
void UnloadScreen(lvgl_runtime& runtime,size_t screen)
{
    auto& state=*runtime.screens.at(screen);if(!state.object){state.pending_unload=false;return;}
    if(runtime.navigation_target==screen || lv_display_get_screen_active(runtime.display)==state.object || lv_display_get_screen_prev(runtime.display)==state.object)
        throw std::invalid_argument("The active or transitioning screen cannot be unloaded");
    Serialization::ResourceScope resources(runtime.resources);Serialization::AssetScope assets(runtime);
    json records=json::array();
    for(const auto& original:state.definition.at("obj")) {
        const auto found=state.objects.find(original.at("oid").get<int>());
        if(found==state.objects.end()||!found->second)continue;
        auto saved=SnapshotObject(found->second);
        for(const auto* key:{"oid","par","hostPath","hostIndex"})if(original.contains(key))saved[key]=original.at(key);
        records.push_back(std::move(saved));
    }
    state.definition["obj"]=std::move(records);
    state.definition["canvas"]=Serialization::Screen::ToJSON(state.object,state.width,state.height);
    state.managed_unload=true;lv_obj_delete(state.object);state.interactions.reset();state.objects.clear();state.focus.clear();state.pending_unload=false;
    for(auto& animation:runtime.animations)if(!animation->object&&animation->timer){lv_timer_delete(animation->timer);animation->timer=nullptr;}
    runtime.animations.erase(std::remove_if(runtime.animations.begin(),runtime.animations.end(),[](const std::unique_ptr<Animation>& item){return !item->object;}),runtime.animations.end());
    ++runtime.screen_unloads;
}
void ScheduleUnloads(lvgl_runtime& runtime)
{
    if(!runtime.unload_inactive)return;
    for(size_t i=0;i<runtime.screens.size();++i)runtime.screens[i]->pending_unload=runtime.screens[i]->object&&i!=runtime.navigation_target;
    if(runtime.unload_timer)return;
    runtime.unload_timer=lv_timer_create([](lv_timer_t* timer){
        auto* owner=static_cast<lvgl_runtime*>(lv_timer_get_user_data(timer));bool pending=false;
        for(size_t i=0;i<owner->screens.size();++i) {
            auto& state=*owner->screens[i];if(!state.pending_unload||!state.object)continue;
            if(lv_display_get_screen_active(owner->display)==state.object){pending=true;continue;}
            if(lv_display_get_screen_prev(owner->display)==state.object){pending=true;continue;}
            try {UnloadScreen(*owner,i);}catch(const std::exception& error){owner->error=error.what();state.pending_unload=false;}
        }
        if(!pending){owner->unload_timer=nullptr;lv_timer_delete(timer);}
    },20,&runtime);
}
} // namespace
const void *lvgl_runtime::Resolve(const json &asset)
{
    if (asset.is_object() && asset.size() == 1 && asset.contains("projectImage"))
    {
        auto found = imageIds.find(asset.at("projectImage").get<std::string>());
        if (found == imageIds.end())
            throw std::invalid_argument("Unknown scoped project image");
        return found->second;
    }
    auto key = asset.dump();
    auto found = images.find(key);
    if (found != images.end())
        return &found->second->frames[0];
    auto resource = std::make_unique<Image>();
    resource->asset = asset;
#if LVGL_VERSION_MAJOR == 8
    if(asset.contains("deviceFrames") && !asset.contains("previewFrames")) {
        const auto& stored=asset.at("deviceFrames");
        if(!stored.is_array() || stored.empty())throw std::invalid_argument("Image has no device frames");
        resource->frames.resize(stored.size());resource->bytes.resize(stored.size());
        for(size_t i=0;i<stored.size();++i) {
            auto f=stored[i];if(f.value("formatVersion",9)!=9)throw std::invalid_argument("Unknown device frame format");
            if(f.value("flags",0u)&8)throw std::invalid_argument("Compressed LVGL 8 images require an embedded preview");
            f["formatVersion"]=9;CGeneration::NormalizePixels(f,false);
            auto w=f.at("w").get<unsigned>(),h=f.at("h").get<unsigned>();
            if(w>2047 || h>2047)throw std::invalid_argument("LVGL 8 image exceeds native dimensions");
            resource->bytes[i]=base64_decode(f.at("data"));auto& d=resource->frames[i];
            d.header.w=w;d.header.h=h;d.header.cf=LV_IMG_CF_TRUE_COLOR_ALPHA;
            d.data=(const uint8_t*)resource->bytes[i].data();d.data_size=(uint32_t)resource->bytes[i].size();
            resource->delays.push_back(asset.contains("delays") && i<asset["delays"].size() ? (std::max)(20u,asset["delays"][i].get<unsigned>()) : 100u);
        }
    } else
#endif
#if LVGL_VERSION_MAJOR >= 9
    if (asset.contains("deviceFrames"))
    {
        const auto &frames = asset.at("deviceFrames");
        if (!frames.is_array() || frames.empty())
            throw std::invalid_argument("Image has no device frames");
        resource->bytes.resize(frames.size());
        resource->frames.resize(frames.size());
        for (size_t i = 0; i < frames.size(); ++i)
        {
            const auto &f = frames[i];
            auto w = f.at("w").get<uint32_t>(), h = f.at("h").get<uint32_t>(),
                 stride = f.at("stride").get<uint32_t>(), cf = f.at("cf").get<uint32_t>();
            if (!w || !h || w > 65535 || h > 65535 || stride > 65535 || cf > 255)
                throw std::invalid_argument("Invalid device image dimensions or format");
            auto &bytes = resource->bytes[i];
            bytes = base64_decode(f.at("data").get<std::string>());
            if (bytes.empty() || bytes.size() > UINT32_MAX)
                throw std::invalid_argument("Invalid device image data");
            auto flags = f.value("flags", 0u);
            if (f.value("formatVersion", 9) != 9)
                throw std::invalid_argument("Image frame targets a different LVGL image format");
            if (cf != LV_COLOR_FORMAT_RAW && cf != LV_COLOR_FORMAT_RAW_ALPHA)
            {
                auto bpp = lv_color_format_get_bpp((lv_color_format_t)cf);
                if (!bpp || stride < (uint64_t(w) * bpp + 7) / 8)
                    throw std::invalid_argument("Image stride is too small");
                uint64_t expected = uint64_t(stride) * h + LV_COLOR_INDEXED_PALETTE_SIZE(cf) * 4;
                if (cf == LV_COLOR_FORMAT_RGB565A8)
                    expected += uint64_t(stride / 2) * h;
                if (flags & LV_IMAGE_FLAGS_COMPRESSED)
                {
#if !LV_BIN_DECODER_RAM_LOAD
                    throw std::invalid_argument("Compressed images require LV_BIN_DECODER_RAM_LOAD");
#endif
                    auto word = [&](size_t offset) {
                        uint32_t value = 0;
                        for (size_t n = 0; n < 4; ++n)
                            value |= uint32_t((uint8_t)bytes[offset + n]) << (8 * n);
                        return value;
                    };
                    if (bytes.size() < 12 || word(4) != bytes.size() - 12 || word(8) != expected ||
                        (word(0) & 15) < 1 || (word(0) & 15) > 2)
                        throw std::invalid_argument("Invalid compressed image header or length");
#if !LV_USE_RLE
                    if ((word(0)&15)==1) throw std::invalid_argument("Image requires LV_USE_RLE");
#endif
#if !LV_USE_LZ4
                    if ((word(0)&15)==2) throw std::invalid_argument("Image requires LV_USE_LZ4");
#endif
                }
                else if (bytes.size() != expected)
                    throw std::invalid_argument("Image pixel data length does not match its dimensions");
            }
            auto &d = resource->frames[i];
            #if LVGL_VERSION_MAJOR >= 9
            d.header.magic = LV_IMAGE_HEADER_MAGIC;
            #endif
            d.header.cf = cf;
            d.header.flags = f.value("flags", 0u);
            d.header.w = w;
            d.header.h = h;
            d.header.stride = stride;
            d.data = (const uint8_t *)bytes.data();
            d.data_size = (uint32_t)bytes.size();
            resource->delays.push_back(asset.contains("delays") && i < asset["delays"].size()
                                           ? (std::max<uint32_t>)(20u, asset["delays"][i].get<uint32_t>())
                                           : 100u);
        }
    }
    else
#endif
    {
        auto decoded = Serialization::ImageAsset::Decode(asset);
#if LVGL_VERSION_MAJOR == 8
        if(decoded.width>2047 || decoded.height>2047)throw std::invalid_argument("LVGL 8 image exceeds native dimensions");
#endif
        auto size = size_t(decoded.width) * decoded.height * 4;
        resource->bytes.resize(decoded.delays.size());
        resource->frames.resize(decoded.delays.size());
        resource->delays = std::move(decoded.delays);
        for (size_t i = 0; i < resource->frames.size(); ++i)
        {
            auto &bytes = resource->bytes[i];
            bytes.assign((const char *)decoded.pixels.data() + i * size, size);
            auto &d = resource->frames[i];
            #if LVGL_VERSION_MAJOR >= 9
            d.header.magic = LV_IMAGE_HEADER_MAGIC;
            #endif
            d.header.cf =
#if LVGL_VERSION_MAJOR == 8
                LV_IMG_CF_TRUE_COLOR_ALPHA;
#else
                LV_COLOR_FORMAT_ARGB8888;
#endif
            d.header.w = decoded.width;
            d.header.h = decoded.height;
#if LVGL_VERSION_MAJOR >= 9
            d.header.stride = decoded.width * 4;
#endif
            d.data = (const uint8_t *)bytes.data();
            d.data_size = (uint32_t)bytes.size();
        }
    }
    auto *result = &resource->frames[0];
    images.emplace(std::move(key), std::move(resource));
    return result;
}
lvgl_runtime::~lvgl_runtime()
{
    if(unload_timer)lv_timer_delete(unload_timer);
    for (auto &a : animations)
        if (a->timer)
            lv_timer_delete(a->timer);
    if (display)
    {
        DisplayScope scope(display);
        CancelLegacyScreenTransition(display);
        auto *active = lv_screen_active();
        bool owned = false;
        for (auto &screen : screens)
            if (screen->object == active)
                owned = true;
        if (owned)
            lv_screen_load(previous ? previous : lv_obj_create(nullptr));
        for (auto &screen : screens)
            if (screen->object)
                lv_obj_delete(screen->object);
    }
    if (previous)
        lv_obj_remove_event_cb_with_user_data(previous, Deleted, &previous);
}
extern "C"
{
    static bool LoadMemory(const void *bytes, size_t size, const lvgl_runtime_options_t *options,
                           lvgl_runtime_t **output, char *error, size_t error_size, uint32_t flags)
    {
        if (output)
            *output = nullptr;
        const uint64_t started=Microseconds();
        try
        {
            if(flags & ~(LVGL_RUNTIME_LAZY_SCREENS|LVGL_RUNTIME_UNLOAD_INACTIVE|LVGL_RUNTIME_BORROW_ASSETS))throw std::invalid_argument("Unknown runtime loading flags");
            if (!output || !bytes || !size)
                throw std::invalid_argument("Input and output must be nonnull");
            auto limit = options && options->max_input_bytes ? options->max_input_bytes : 16u * 1024u * 1024u;
            if (size > limit)
                throw std::invalid_argument("Project exceeds the input size limit");
            if (!lv_is_initialized())
                throw std::invalid_argument("Initialize LVGL first");
            auto *display = options && options->display ? static_cast<lv_display_t *>(options->display)
                                                        : lv_display_get_default();
            if (!display)
                throw std::invalid_argument("Create an LVGL display first");
            // The XML codec restores the same typed record directly, without
            // an intermediate XML -> Protobuf buffer or another decode pass.
            // Protobuf trie payloads stay as trusted input spans until final
            // catalog placement. XML text must never become borrowed assets.
            const bool xml = IsXmlInput(bytes, size);
            Serialization::BinaryRecord record;
            if (xml) {
#if LVSERIAL_USE_XML
                record = Serialization::Xml::Decode(bytes, size);
#else
                throw std::invalid_argument("XML input is disabled in this runtime; enable LVSERIAL_USE_XML");
#endif
            }
            else record = Serialization::Protobuf::DecodeBorrowedAssets(bytes, size);
            if (record.kind != Serialization::EntityKind::Project || record.type != "project")
                throw std::invalid_argument("Expected a project record");
            auto document = std::move(record.data);
            std::map<std::string,size_t> trieSourceIndices;
            if(document.contains("advancedKeyboard") && !record.borrowedAssets.empty() && document.contains("tries")) {
                size_t index=0;
                for(const auto& asset:document["tries"])trieSourceIndices.emplace(asset.at("id").get<std::string>(),index++);
            }
            Serialization::ProjectDesign::Materialize(document,true);
            if(!trieSourceIndices.empty()) {
                // Pruning the project model library changes its array indices.
                // Preserve zero-copy spans, rebasing only trusted decoder paths.
                Serialization::BorrowedAssets retained;
                size_t index=0;
                if(document.contains("tries"))for(auto& asset:document["tries"]) {
                    const auto source=trieSourceIndices.find(asset.at("id").get<std::string>());
                    if(source==trieSourceIndices.end())throw std::invalid_argument("Unknown materialized trie source");
                    for(const auto* key:{"data","indexData"})if(asset.contains(key)) {
                        auto& value=asset[key];
                        if(!value.is_object() || !value.contains("borrowedBytes"))continue;
                        const auto oldPath="/tries/"+std::to_string(source->second)+"/"+key;
                        const auto newPath="/tries/"+std::to_string(index)+"/"+key;
                        const auto span=record.borrowedAssets.find(oldPath);
                        if(value.at("borrowedBytes")!=oldPath || span==record.borrowedAssets.end())
                            throw std::invalid_argument("Invalid borrowed trie source");
                        value["borrowedBytes"]=newPath;retained.emplace(newPath,span->second);
                    }
                    ++index;
                }
                record.borrowedAssets=std::move(retained);
            }
            Serialization::MigrateAdvancedKeyboardEditors(document);
            auto tries = (!xml && (flags & LVGL_RUNTIME_BORROW_ASSETS))
                ? Serialization::ProjectTries::Load(document, record.borrowedAssets)
                : Serialization::ProjectTries::LoadOwned(document, record.borrowedAssets);
            Serialization::ProjectTries::Scope trieScope(tries);
            Validate(document);
            RuntimeValidation::ValidateFeatures(document);
#if LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 6
            CGeneration::PrepareTarget(document, LVGL_RUNTIME_VERSION, true);
            ConvertCoordinates(document);
#endif
            RuntimeValidation::ValidateFonts(document);
            static const json emptyArray = json::array();
            Serialization::ProjectFonts::References(document, document.contains("fonts") ? document.at("fonts") : emptyArray, false);
            Serialization::ProjectImages::References(document, document.contains("images") ? document.at("images") : emptyArray,
                                                     false);
            auto runtime = std::make_unique<lvgl_runtime>();
            runtime->display = display;runtime->tries=tries;runtime->input_group=lv_group_get_default();
            runtime->input_bytes=size;runtime->lazy=(flags&LVGL_RUNTIME_LAZY_SCREENS)!=0;
            runtime->unload_inactive=(flags&LVGL_RUNTIME_UNLOAD_INACTIVE)!=0;
            DisplayScope displayScope(display);
            Track(lv_screen_active(), &runtime->previous);
            Serialization::ResourceScope resources(runtime->resources);
            Serialization::AssetScope assets(*runtime);
            for (const auto &font : document.contains("fonts") ? document.at("fonts") : emptyArray)
            {
                auto *restored = &Serialization::LVFont::FromJSON(font.at("font"));
                runtime->fonts[font.at("id")] = restored;
                runtime->resources.fontRefs[font.at("id")] = restored;
            }
            for (const auto &image : document.contains("images") ? document.at("images") : emptyArray)
                runtime->imageIds[image.at("id")] =
                    static_cast<const lv_image_dsc_t *>(runtime->Resolve(image.at("image")));
            const size_t initial=document.value("activeScreen",size_t(0));
            auto variableRecords = document.contains("variables") ? std::move(document.at("variables")) : json::array();
            document.erase("variables");
            json screens;
            if (document.contains("screens")) screens = std::move(document.at("screens"));
            else { screens = json::array(); screens.push_back(std::move(document)); }
            runtime->screens.reserve(screens.size());
            for(auto& screen:screens) {
                runtime->screens.push_back(std::make_unique<Screen>());auto& state=*runtime->screens.back();
                int next=0;for(auto& widget:screen["obj"]) {
                    widget["oid"]=widget.value("oid",next++);
                    const auto events=widget.value("base",json::object()).value("events",json::array());
                    Serialization::InteractionActions::Normalize(events);
                }
                auto canvas=screen.value("canvas",screen.value("screen",json::object()));
                const auto dimensions=Serialization::Screen::Normalize(canvas);
                state.width=dimensions.at("width");state.height=dimensions.at("height");state.definition=std::move(screen);
                for(const auto* key:{"fonts","images","variables","tries","components","projectTheme","sharedStyles","screens"})state.definition.erase(key);
                // Decoding grows obj arrays geometrically. Moving the tree
                // avoids deep copies; compact just these shallow JSON handles.
                state.definition.at("obj").get_ref<json::array_t&>().shrink_to_fit();
            }
            if(!runtime->lazy)for(size_t i=0;i<runtime->screens.size();++i)EnsureScreen(*runtime,i);
            else if(!options||options->load_initial_screen)EnsureScreen(*runtime,initial);
            std::set<std::string> names;
            for (const auto &item :
                 Serialization::LVVariable::NormalizeList(variableRecords))
            {
                auto v = std::make_unique<Variable>();
                v->owner = runtime.get();
                v->definition = Serialization::LVVariable::FromJSON(item);
                if (v->definition.name.empty() || v->definition.name.find('\0') != std::string::npos ||
                    !names.insert(v->definition.name).second)
                    throw std::invalid_argument("Variable names must be unique and nonempty");
                runtime->variables.push_back(std::move(v));
                auto &loaded = *runtime->variables.back();
                for(const auto& definition:Serialization::LVVariable::Bindings(loaded.definition)) {
                    auto binding=std::make_unique<VariableBindingState>();binding->variable=&loaded;binding->definition=definition;
                    loaded.bindings.push_back(std::move(binding));Bind(*loaded.bindings.back());
                }
                runtime->updating = true;
                Assign(loaded, std::move(loaded.definition.defaultValue), false);
                for (auto &x : loaded.definition.values)
                    Assign(loaded, std::move(x), true);
                loaded.definition.values = json::array();
                loaded.definition.extra.erase("value");
                loaded.definition.defaultValue = nullptr;
                loaded.definition.extra.erase("default");
                runtime->updating = false;
            }
            runtime->initialized=true;
            for(size_t si=0;si<runtime->screens.size();++si)if(runtime->screens[si]->object)InstallInteractions(*runtime,si);
            if(!options||options->load_initial_screen){lv_screen_load(runtime->screens[initial]->object);ActivateFocus(*runtime,initial);}
            else {if(runtime->previous)lv_screen_load(runtime->previous);ActivateFocus(*runtime,SIZE_MAX);}
            runtime->load_us=Microseconds()-started;
            *output = runtime.release();
            Error(error, error_size, "");
            return true;
        }
        catch (const std::exception &e)
        {
            Error(error, error_size, e.what());
            return false;
        }
        catch (...)
        {
            Error(error, error_size, "Unknown runtime loading error");
            return false;
        }
    }
    bool lvgl_runtime_load_memory(const void *bytes, size_t size, const lvgl_runtime_options_t *options,
                                  lvgl_runtime_t **output, char *error, size_t error_size)
    {
        return LoadMemory(bytes, size, options, output, error, error_size, 0);
    }
    bool lvgl_runtime_load_memory_ex(const void* bytes,size_t size,const lvgl_runtime_options_t* options,
        uint32_t flags,lvgl_runtime_t** output,char* error,size_t error_size)
    {
        return LoadMemory(bytes,size,options,output,error,error_size,flags);
    }
    bool lvgl_runtime_load_memory_borrowed_assets(const void *bytes, size_t size,
        const lvgl_runtime_options_t *options, lvgl_runtime_t **output, char *error, size_t error_size)
    {
        return LoadMemory(bytes, size, options, output, error, error_size, LVGL_RUNTIME_BORROW_ASSETS);
    }
    bool lvgl_runtime_load_file(const char *path, const lvgl_runtime_options_t *options,
                                lvgl_runtime_t **output, char *error, size_t error_size)
    {
        if (output)
            *output = nullptr;
        lv_fs_file_t file{};
        bool open = false;
        try
        {
            if (!path || lv_fs_open(&file, path, LV_FS_MODE_RD) != LV_FS_RES_OK)
                throw std::runtime_error("Cannot open project file through LVGL filesystem");
            open = true;
            uint32_t size = 0;
            if (lv_fs_seek(&file, 0, LV_FS_SEEK_END) != LV_FS_RES_OK ||
                lv_fs_tell(&file, &size) != LV_FS_RES_OK ||
                lv_fs_seek(&file, 0, LV_FS_SEEK_SET) != LV_FS_RES_OK)
                throw std::runtime_error("Cannot seek project file");
            auto limit = options && options->max_input_bytes ? options->max_input_bytes : 16u * 1024u * 1024u;
            if (!size || size > limit)
                throw std::runtime_error("Invalid file size or project exceeds input limit");
            std::vector<uint8_t> data(size);
            uint32_t read = 0;
            if (lv_fs_read(&file, data.data(), size, &read) != LV_FS_RES_OK || read != size)
                throw std::runtime_error("Cannot read complete project file");
            lv_fs_close(&file);
            open = false;
            return lvgl_runtime_load_memory(data.data(), data.size(), options, output, error, error_size);
        }
        catch (const std::exception &e)
        {
            if (open)
                lv_fs_close(&file);
            Error(error, error_size, e.what());
            return false;
        }
    }
    void lvgl_runtime_destroy(lvgl_runtime_t *runtime)
    {
        delete runtime;
    }
    const char *lvgl_runtime_backend(void)
    {
        return LVGL_RUNTIME_VERSION;
    }
    const char *lvgl_runtime_error(const lvgl_runtime_t *runtime)
    {
        return runtime ? runtime->error.c_str() : "Runtime is null";
    }
    size_t lvgl_runtime_screen_count(const lvgl_runtime_t *runtime)
    {
        return runtime ? runtime->screens.size() : 0;
    }
    lv_obj_t *lvgl_runtime_screen(const lvgl_runtime_t *runtime, size_t screen)
    {
        return runtime && screen < runtime->screens.size() ? runtime->screens[screen]->object : nullptr;
    }
    bool lvgl_runtime_screen_size(const lvgl_runtime_t *runtime, size_t screen, int32_t *width, int32_t *height)
    {
        if (!runtime || screen >= runtime->screens.size() || !width || !height) return false;
        *width = runtime->screens[screen]->width;
        *height = runtime->screens[screen]->height;
        return true;
    }
    bool lvgl_runtime_ensure_screen(lvgl_runtime_t* runtime,size_t screen)
    {
        if(!runtime)return false;
        try {EnsureScreen(*runtime,screen);runtime->error.clear();return true;}
        catch(const std::exception& error){runtime->error=error.what();return false;}
    }
    bool lvgl_runtime_unload_screen(lvgl_runtime_t* runtime,size_t screen)
    {
        if(!runtime)return false;
        try {if(screen>=runtime->screens.size())throw std::invalid_argument("Screen index is outside the project");UnloadScreen(*runtime,screen);runtime->error.clear();return true;}
        catch(const std::exception& error){runtime->error=error.what();return false;}
    }
    size_t lvgl_runtime_active_screen(const lvgl_runtime_t* runtime)
    {
        if(runtime)for(size_t i=0;i<runtime->screens.size();++i)
            if(runtime->screens[i]->object&&runtime->screens[i]->object==lv_display_get_screen_active(runtime->display))return i;
        return SIZE_MAX;
    }
    size_t lvgl_runtime_loaded_screen_count(const lvgl_runtime_t* runtime)
    {
        size_t count=0;if(runtime)for(const auto& screen:runtime->screens)if(screen->object)++count;return count;
    }
    size_t lvgl_runtime_back_count(const lvgl_runtime_t* runtime){return runtime?runtime->back.size():0;}
    bool lvgl_runtime_show_screen_animated(lvgl_runtime_t* runtime,size_t screen,lvgl_runtime_transition_t transition,uint32_t duration)
    {
        if(!runtime)return false;
        try {
            if(transition<LVGL_RUNTIME_TRANSITION_NONE||transition>LVGL_RUNTIME_TRANSITION_DOWN||duration>2000)
                throw std::invalid_argument("Choose a supported transition and duration from 0 to 2000 ms");
            const auto previous=lvgl_runtime_active_screen(runtime);
            CancelLegacyScreenTransition(runtime->display);
            EnsureScreen(*runtime,screen);
            DisplayScope displayScope(runtime->display);runtime->navigation_target=screen;
#if LVGL_VERSION_MAJOR == 8 || (LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 5)
            static const decltype(LV_SCR_LOAD_ANIM_NONE) animations[]={LV_SCR_LOAD_ANIM_NONE,LV_SCR_LOAD_ANIM_FADE_ON,LV_SCR_LOAD_ANIM_MOVE_LEFT,LV_SCR_LOAD_ANIM_MOVE_RIGHT,LV_SCR_LOAD_ANIM_MOVE_TOP,LV_SCR_LOAD_ANIM_MOVE_BOTTOM};
#else
            static const lv_screen_load_anim_t animations[]={LV_SCREEN_LOAD_ANIM_NONE,LV_SCREEN_LOAD_ANIM_FADE_IN,LV_SCREEN_LOAD_ANIM_MOVE_LEFT,LV_SCREEN_LOAD_ANIM_MOVE_RIGHT,LV_SCREEN_LOAD_ANIM_MOVE_TOP,LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM};
#endif
            if(transition==LVGL_RUNTIME_TRANSITION_NONE||!duration||previous==SIZE_MAX)lv_screen_load(runtime->screens[screen]->object);
            else lv_screen_load_anim(runtime->screens[screen]->object,animations[transition],duration,0,false);
            if(previous!=SIZE_MAX&&previous!=screen){if(runtime->back.size()==128)runtime->back.erase(runtime->back.begin());runtime->back.push_back(previous);}
            ActivateFocus(*runtime,screen);ScheduleUnloads(*runtime);runtime->error.clear();return true;
        }catch(const std::exception& error){runtime->error=error.what();return false;}
    }
    bool lvgl_runtime_show_screen(lvgl_runtime_t* runtime,size_t screen)
    {
        if(!runtime||screen>=runtime->screens.size())return false;
        try {
            const auto transition=runtime->screens[screen]->definition.value("transition",json::object());
            const auto type=transition.value("type",std::string("none"));
            lvgl_runtime_transition_t value=type=="fade"?LVGL_RUNTIME_TRANSITION_FADE:type=="slide_left"?LVGL_RUNTIME_TRANSITION_LEFT:
                type=="slide_right"?LVGL_RUNTIME_TRANSITION_RIGHT:type=="slide_up"?LVGL_RUNTIME_TRANSITION_UP:type=="slide_down"?LVGL_RUNTIME_TRANSITION_DOWN:LVGL_RUNTIME_TRANSITION_NONE;
            return lvgl_runtime_show_screen_animated(runtime,screen,value,transition.value("duration",uint32_t(0)));
        }catch(const std::exception& error){runtime->error=error.what();return false;}
    }
    bool lvgl_runtime_back(lvgl_runtime_t* runtime)
    {
        if(!runtime||runtime->back.empty())return false;
        auto history=runtime->back;const auto target=history.back();history.pop_back();
        if(!lvgl_runtime_show_screen(runtime,target))return false;
        runtime->back=std::move(history);return true;
    }
    bool lvgl_runtime_get_stats(const lvgl_runtime_t* runtime,lvgl_runtime_stats_t* stats)
    {
        if(!runtime||!stats)return false;
        *stats={};stats->input_bytes=runtime->input_bytes;stats->screen_count=runtime->screens.size();
        stats->loaded_screens=lvgl_runtime_loaded_screen_count(runtime);stats->load_us=runtime->load_us;
        stats->last_screen_load_us=runtime->last_screen_load_us;stats->screen_loads=runtime->screen_loads;stats->screen_unloads=runtime->screen_unloads;
        for(const auto& screen:runtime->screens)for(const auto& object:screen->objects)if(object.second)++stats->loaded_objects;
        lv_mem_monitor_t memory{};lv_mem_monitor(&memory);stats->lvgl_heap_available=memory.total_size!=0;
        if(stats->lvgl_heap_available){stats->lvgl_heap_used=memory.total_size-memory.free_size;stats->lvgl_heap_peak=memory.max_used;stats->lvgl_heap_largest_free=memory.free_biggest_size;}
        return true;
    }
    lv_obj_t *lvgl_runtime_object(const lvgl_runtime_t *runtime, size_t screen, int32_t id)
    {
        if (!runtime || screen >= runtime->screens.size())
            return nullptr;
        auto found = runtime->screens[screen]->objects.find(id);
        return found == runtime->screens[screen]->objects.end() ? nullptr : found->second;
    }
    const lv_font_t *lvgl_runtime_font(const lvgl_runtime_t *runtime, const char *id)
    {
        if (!runtime || !id)
            return nullptr;
        auto found = runtime->fonts.find(id);
        return found == runtime->fonts.end() ? nullptr : found->second;
    }
    const void *lvgl_runtime_image(const lvgl_runtime_t *runtime, const char *id)
    {
        if (!runtime || !id)
            return nullptr;
        auto found = runtime->imageIds.find(id);
        return found == runtime->imageIds.end() ? nullptr : found->second;
    }
    size_t lvgl_runtime_variable_count(const lvgl_runtime_t *runtime)
    {
        return runtime ? runtime->variables.size() : 0;
    }
    const char *lvgl_runtime_variable_name(const lvgl_runtime_t *runtime, size_t index)
    {
        return runtime && index < runtime->variables.size()
                   ? runtime->variables[index]->definition.name.c_str()
                   : nullptr;
    }
    bool lvgl_runtime_get_variable(lvgl_runtime_t *runtime, const char *name, lvgl_runtime_value_t *value)
    {
        auto *v = Find(runtime, name);
        if (!v || !value)
            return false;
        ExportValue(*v, v->current, value);
        return true;
    }
    bool lvgl_runtime_set_variable(lvgl_runtime_t *runtime, const char *name,
                                   const lvgl_runtime_value_t *value)
    {
        auto *v = Find(runtime, name);
        if (!v || !value)
            return false;
        if(runtime->updating){runtime->error="Cannot set a variable recursively during widget propagation";return false;}
        try
        {
            if (value->type != (lvgl_runtime_value_type_t)v->definition.type)
                throw std::invalid_argument("Variable type mismatch");
            bool previous = runtime->updating;
            runtime->updating = true;
            try
            {
                Assign(*v, ImportValue(*value), true);
            }
            catch (...)
            {
                runtime->updating = previous;
                throw;
            }
            runtime->updating = previous;
            runtime->error.clear();
            return true;
        }
        catch (const std::exception &e)
        {
            runtime->error = e.what();
            return false;
        }
    }
    size_t lvgl_runtime_history_count(const lvgl_runtime_t *runtime, const char *name)
    {
        auto *v = Find(const_cast<lvgl_runtime_t *>(runtime), name);
        return v ? (v->definition.history == 1 ? v->history_start : v->history.size()) : 0;
    }
    bool lvgl_runtime_history_get(lvgl_runtime_t *runtime, const char *name, size_t index,
                                  lvgl_runtime_value_t *value)
    {
        auto *v = Find(runtime, name);
        if (!v || !value || index >= (v->definition.history == 1 ? v->history_start : v->history.size()))
            return false;
        ExportValue(*v, v->definition.history == 1 ? v->current :
            v->history[(v->history_start + index) % v->history.size()], value);
        return true;
    }
}
