#include "InteractionActions.h"
#include "ObjectUserData.h"
#include "CustomWidgets.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <utility>

namespace Serialization::InteractionActions {
namespace {
bool Has(const std::set<std::string>& values, const std::string& value) { return values.count(value) != 0; }
int Index(const json& item, const char* key, int fallback) {
    auto found=item.find(key);if(found==item.end())return fallback;
    if(!found->is_number_integer() || (found->is_number_unsigned() && found->get<uint64_t>()>INT32_MAX) || (!found->is_number_unsigned() && (found->get<int64_t>()<INT32_MIN || found->get<int64_t>()>INT32_MAX)))throw std::invalid_argument(std::string(key)+" must be a supported integer ID");
    return found->get<int>();
}
bool Truth(const json& x) { return x.is_boolean() ? x.get<bool>() : x.is_number() ? x.get<double>() != 0 : x.is_string() ? !x.get_ref<const std::string&>().empty() : !x.is_null(); }
struct Reference { lv_obj_t* object = nullptr; };
struct References { lv_obj_t* source; std::vector<std::unique_ptr<Reference>> targets; };
std::map<lv_obj_t*, std::unique_ptr<References>>& Registry() { static std::map<lv_obj_t*, std::unique_ptr<References>> value; return value; }
void TargetDeleted(lv_event_t* e) { static_cast<Reference*>(lv_event_get_user_data(e))->object = nullptr; }
void ReleaseReferences(lv_obj_t* source) {
    auto found = Registry().find(source); if (found == Registry().end()) return;
    for (auto& target : found->second->targets) if (target->object) lv_obj_remove_event_cb_with_user_data(target->object, TargetDeleted, target.get());
    Registry().erase(found);
}
void SourceDeleted(lv_event_t* e) { ReleaseReferences(static_cast<lv_obj_t*>(lv_event_get_current_target(e))); }
json Stored(lv_obj_t* object) {
    auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
    return data && data->objectJson.contains("base") ? data->objectJson["base"].value("events", json::array()) : json::array();
}
std::string Text(lv_obj_t* object) {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TEXTAREA
    if (lv_obj_has_class(object, &lv_textarea_class)) return lv_textarea_get_text(object);
    if (CustomWidgets::Type(object) && std::string(CustomWidgets::Type(object)) == "textBox")
        return CustomWidgets::ToJSON(object).at("textBox").value("text", std::string{});
#endif
    throw std::invalid_argument("Validation target must be a text box");
}
}
json Normalize(const json& input) {
    if (!input.is_array()) throw std::invalid_argument("Events must be an array");
    if (input.size() > 256) throw std::invalid_argument("A widget supports at most 256 actions");
    json output = json::array();
    for (const auto& item : input) {
        if (!item.is_object()) throw std::invalid_argument("Each event action must be an object");
        const auto event = item.value("event", std::string("clicked"));
        const auto action = item.value("action", std::string{});
        if (!Has({"clicked", "value_changed", "ready"}, event)) throw std::invalid_argument("Unknown interaction event: " + event);
        if (!Has({"show", "hide", "toggle", "navigate", "set_variable", "increment_variable", "validate"}, action)) throw std::invalid_argument("Unknown interaction action: " + action);
        json record = {{"event", event}, {"action", action}};
        if (action == "navigate") { auto screen = Index(item,"screen",0); if (screen < 0) throw std::invalid_argument("Screen index cannot be negative"); record["screen"] = screen; }
        else if (action == "set_variable" || action == "increment_variable") {
            auto variable = item.value("variable", std::string{}); if (variable.empty()) throw std::invalid_argument("Select a variable for this action");
            record["variable"] = variable; record["value"] = item.value("value", json(action == "increment_variable" ? 1 : 0));
            if(record["value"].is_string() && record["value"].get_ref<const std::string&>().find('\0')!=std::string::npos)throw std::invalid_argument("Action values cannot contain NUL characters");
            if (action == "increment_variable" && (!record["value"].is_number() || !std::isfinite(record["value"].get<double>()))) throw std::invalid_argument("Increment must be a finite number");
            if(action == "increment_variable" && record["value"].is_number_unsigned() && record["value"].get<uint64_t>()>INT64_MAX)throw std::invalid_argument("Increment exceeds int64 range");
        } else {
            const int target=Index(item,"targetOid",-1);if(target < -2)throw std::invalid_argument("Invalid action target ID");record["targetOid"] = target;
            if (item.contains("targetScreen") && target!=-1) { int screen = Index(item,"targetScreen",0); if (screen < 0) throw std::invalid_argument("Target screen cannot be negative"); record["targetScreen"] = screen; }
            if (action == "validate") {
                record["required"] = item.value("required", true);
                for (auto key : {"min", "max"}) if (item.contains(key)) { if (!item.at(key).is_number() || !std::isfinite(item.at(key).get<double>())) throw std::invalid_argument("Validation bounds must be finite numbers"); record[key] = item.at(key); }
                if (record.contains("min") && record.contains("max") && record["min"].get<double>() > record["max"].get<double>()) throw std::invalid_argument("Minimum exceeds maximum");
            }
        }
        if (item.contains("condition") && !item.at("condition").is_null() && !item.at("condition").empty()) {
            const auto& c = item.at("condition"); auto variable = c.value("variable", std::string{}), op = c.value("op", std::string("eq"));
            if (variable.empty() || !Has({"eq", "ne", "gt", "lt", "truthy"}, op)) throw std::invalid_argument("Invalid action condition");
            record["condition"] = {{"variable", variable}, {"op", op}, {"value", c.value("value", json(true))}};
        }
        output.push_back(std::move(record));
    }
    return output;
}
bool ConditionMatches(const json& c, const std::function<json(const std::string&)>& read) {
    if (c.is_null() || c.empty()) return true;
    if (!read) return false;
    const auto actual = read(c.at("variable")), value = c.value("value", json(true)); const auto op = c.value("op", std::string("eq"));
    if (op == "truthy") return Truth(actual);
    if (op == "eq") return actual == value;
    if (op == "ne") return actual != value;
    if (!actual.is_number() || !value.is_number()) return false;
    return op == "gt" ? actual.get<double>() > value.get<double>() : actual.get<double>() < value.get<double>();
}
void ResolveReferences(lv_obj_t* object, const std::map<int, lv_obj_t*>& objects) {
    ReleaseReferences(object); auto events = Normalize(Stored(object)); if (events.empty()) return;
    auto refs = std::make_unique<References>(); refs->source = object;
    for (const auto& action : events) {
        auto target = std::make_unique<Reference>(); auto id = action.value("targetOid", -1);
        if (id >= 0 && !action.contains("targetScreen")) { auto found = objects.find(id); if (found != objects.end()) target->object = found->second; }
        if (target->object) lv_obj_add_event_cb(target->object, TargetDeleted, LV_EVENT_DELETE, target.get());
        refs->targets.push_back(std::move(target));
    }
    lv_obj_remove_event_cb(object, SourceDeleted); lv_obj_add_event_cb(object, SourceDeleted, LV_EVENT_DELETE, nullptr);
    Registry()[object] = std::move(refs);
}
void SetEvents(lv_obj_t* object, const json& events, const std::map<int, lv_obj_t*>& objects) {
    if (!object) throw std::invalid_argument("Select a widget first");
    auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object)); if (!data) throw std::invalid_argument("Select an authored widget");
    data->objectJson["base"]["events"] = Normalize(events); ResolveReferences(object, objects);
}
json EventsForSave(lv_obj_t* object, const std::unordered_map<lv_obj_t*, int>& indices) {
    auto events = Normalize(Stored(object)); auto found = Registry().find(object); if (found == Registry().end()) return events;
    for (size_t i = 0; i < events.size() && i < found->second->targets.size(); ++i) {
        auto& action = events[i]; if (!action.contains("targetOid") || action.at("targetOid").get<int>() < 0 || action.contains("targetScreen")) continue;
        auto target = indices.find(found->second->targets[i]->object); action["targetOid"] = target == indices.end() ? -2 : target->second;
    }
    return events;
}
struct Context::Binding { Context* owner; lv_obj_t* source; size_t screen; int oid; json actions; };
Context::Context(Hooks callbacks) : hooks(std::move(callbacks)) {}
Context::~Context() { for (auto& binding : bindings) if (binding->source) lv_obj_remove_event_cb_with_user_data(binding->source, Dispatch, binding.get()); }
void Context::Bind(lv_obj_t* source, size_t screen, int oid, const json& events) {
    auto actions = Normalize(events); if (actions.empty() || !source) return;
    auto binding = std::make_unique<Binding>(Binding{this, source, screen, oid, std::move(actions)});
    lv_obj_add_event_cb(source, Dispatch, LV_EVENT_ALL, binding.get()); bindings.push_back(std::move(binding));
}
void Context::Dispatch(lv_event_t* event) {
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event)); auto* owner = binding->owner;
    const auto code = lv_event_get_code(event); if (code == LV_EVENT_DELETE) { binding->source = nullptr; return; }
    if (lv_event_get_target(event) != lv_event_get_current_target(event) || owner->executing ||
        (owner->hooks.enabled && !owner->hooks.enabled())) return;
    const char* name = code == LV_EVENT_CLICKED ? "clicked" : code == LV_EVENT_VALUE_CHANGED ? "value_changed" : code == LV_EVENT_READY ? "ready" : nullptr;
    if (!name) return;
    owner->executing = true;
    try { for (const auto& action : binding->actions) if (action.at("event") == name && !owner->Execute(*binding, action)) break; }
    catch (const std::exception& e) { if (owner->hooks.error) owner->hooks.error(e.what()); }
    owner->executing = false;
}
bool Context::Execute(Binding& binding, const json& a) {
    if (!ConditionMatches(a.value("condition", json()), hooks.read)) return true;
    auto action = a.at("action").get<std::string>();
    if (action == "navigate") { if (!hooks.navigate || !hooks.navigate(a.at("screen"))) throw std::invalid_argument("Action references a missing screen"); return true; }
    if (action == "set_variable" || action == "increment_variable") {
        if (!hooks.write || !hooks.read) throw std::invalid_argument("Variable actions are unavailable");
        auto variable = a.at("variable").get<std::string>(); auto value = a.at("value");
        if (action == "increment_variable") { auto previous = hooks.read(variable); if (!previous.is_number()) throw std::invalid_argument("Increment requires a numeric variable");
            if (previous.is_number_integer() && value.is_number_integer()) { auto x = previous.get<int64_t>(), y = value.get<int64_t>(); if ((y > 0 && x > INT64_MAX-y) || (y < 0 && x < INT64_MIN-y)) throw std::invalid_argument("Variable increment overflow"); value = x+y; }
            else { double x = previous.get<double>() + value.get<double>(); if (!std::isfinite(x)) throw std::invalid_argument("Variable increment overflow"); value = x; }
        }
        hooks.write(variable, value); return true;
    }
    int id = a.value("targetOid", -1); auto* target = id == -1 ? binding.source : hooks.object ? hooks.object(a.value("targetScreen", binding.screen), id) : nullptr;
    if (!target) throw std::invalid_argument("Action references a missing widget");
    if (action == "show") lv_obj_remove_flag(target, LV_OBJ_FLAG_HIDDEN);
    else if (action == "hide") lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN);
    else if (action == "toggle") { if (lv_obj_has_flag(target, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(target, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN); }
    else if (action == "validate") {
        auto text = Text(target); const auto first = text.find_first_not_of(" \t\r\n"); bool valid = !a.value("required", true) || first != std::string::npos;
        if (valid && first != std::string::npos && (a.contains("min") || a.contains("max"))) { char* end = nullptr; double value = std::strtod(text.c_str(), &end); while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end; valid = end != text.c_str() && end && !*end && std::isfinite(value) && (!a.contains("min") || value >= a.at("min").get<double>()) && (!a.contains("max") || value <= a.at("max").get<double>()); }
        lv_obj_set_style_border_color(target, lv_color_hex(0xd92d20), LV_PART_MAIN | LV_STATE_USER_4); lv_obj_set_style_border_width(target, 2, LV_PART_MAIN | LV_STATE_USER_4);
        if (valid) lv_obj_remove_state(target, LV_STATE_USER_4); else { lv_obj_add_state(target, LV_STATE_USER_4); lv_obj_scroll_to_view_recursive(target, LV_ANIM_OFF); if (hooks.error) hooks.error("Please enter a valid value"); }
        return valid;
    }
    return true;
}
}
