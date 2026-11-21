#pragma once

#include "WidgetRecords.h"
#include "RuntimeFeatures.h"
#include "ExtendedWidgets.h"
#include "Style.h"
#include <map>
#include <set>
#include <stdexcept>
#include <limits>

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
namespace Serialization
{
    // Composite widgets keep their model on the object, independently of the editor.
#if LVSERIAL_USE_TEXTBOX || LVSERIAL_USE_LAYERVIEW
    class CustomWidgets
    {
        struct Row { lv_obj_t* button; lv_obj_t* toggle; lv_obj_t* caption; };
        struct State
        {
            json record;
            bool updating = false;
            std::map<std::string, lv_obj_t*> parts;
            std::map<uint32_t, Row> rows;
        };
        struct Action { lv_obj_t* root; int direction; uint32_t id; bool toggle; };

        static json Defaults(const std::string& type)
        {
            if (type == "textBox") return {{"title", "Value"}, {"text", "0"}, {"value", 0}, {"min", 0}, {"max", 100},
                {"step", 1}, {"numeric", true}, {"readOnly", false}, {"password", false}, {"placeholder", ""},
                {"maxLength", 32}, {"acceptedChars", ""}, {"showButtons", true}, {"titleWidth", 64}, {"buttonWidth", 28},
                {"minusText", LV_SYMBOL_MINUS}, {"plusText", LV_SYMBOL_PLUS}, {"spacing", 6}, {"children", json::array()}};
            return {{"title", "Layer view"}, {"indent", 18}, {"rowHeight", 32}, {"selected", 1}, {"showTitle", true},
                {"nodes", {{{"id", 1}, {"parent", 0}, {"text", "Screen"}, {"expanded", true}, {"disabled", false}},
                           {{"id", 2}, {"parent", 1}, {"text", "Layer 1"}, {"expanded", true}, {"disabled", false}}}},
                {"children", json::array()}};
        }

        static bool Changed(const json& old, const json& record, const char* key)
        { return record.contains(key) && (!old.contains(key) || old[key] != record[key]); }

        static lv_obj_t* Button(lv_obj_t* parent, const char* text)
        {
            auto* button = lv_button_create(parent);
            lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
            auto* label = lv_label_create(button);
            lv_label_set_text(label, text);
            lv_obj_center(label);
            return button;
        }

        static void Event(lv_event_t* event)
        {
            auto* action = static_cast<Action*>(lv_event_get_user_data(event));
            if (lv_event_get_code(event) == LV_EVENT_DELETE) { delete action; return; }
            auto* root = action->root;
            auto& state = WidgetData<State>(root);
            if (state.updating) return;
#if LVSERIAL_USE_TEXTBOX
            if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED && action->direction == 0 && action->id == 0)
            {
                const std::string text = lv_textarea_get_text(state.parts.at("Entry"));
                state.record["text"] = text;
                if (state.record.value("numeric", true))
                {
                    try
                    {
                        size_t used = 0;
                        const auto number = std::stoll(text, &used);
                        if (used == text.size()) state.record["value"] = std::clamp<int64_t>(number,
                            state.record.at("min").get<int32_t>(), state.record.at("max").get<int32_t>());
                    }
                    catch (const std::exception&) { /* Preserve incomplete typed numbers. */ }
                }
                PreserveSection(root, "textBox", state.record);
                lv_obj_send_event(root, LV_EVENT_VALUE_CHANGED, nullptr);
            }
#endif
            if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
            if (action->direction)
            {
                if (state.record.value("readOnly", false) || !state.record.value("numeric", true)) return;
                const auto next = std::clamp<int64_t>(state.record.at("value").get<int64_t>() +
                    action->direction * state.record.at("step").get<int64_t>(), state.record.at("min").get<int32_t>(), state.record.at("max").get<int32_t>());
                auto record = state.record;
                record["value"] = next;
                record["text"] = std::to_string(next);
                Apply(root, record);
                lv_obj_send_event(root, LV_EVENT_VALUE_CHANGED, nullptr);
            }
            else if (action->id)
            {
                auto record = state.record;
                for (auto& node : record["nodes"])
                    if (node.at("id") == action->id)
                    {
                        if (node.value("disabled", false)) return;
                        if (action->toggle) node["expanded"] = !node.value("expanded", true);
                        else record["selected"] = action->id;
                        break;
                    }
                Apply(root, record);
                lv_obj_send_event(root, LV_EVENT_VALUE_CHANGED, nullptr);
            }
        }

        static void Bind(lv_obj_t* part, lv_obj_t* root, int direction = 0, uint32_t id = 0, bool toggle = false)
        { lv_obj_add_event_cb(part, Event, LV_EVENT_ALL, new Action{root, direction, id, toggle}); }

        static void ValidateNodes(json& record)
        {
            if (!record["nodes"].is_array()) throw std::invalid_argument("Layer nodes must be an array");
            std::map<uint32_t, uint32_t> parents;
            uint32_t next = 1;
            for (const auto& node : record["nodes"])
            {
                const auto id = node.value("id", 0u);
                if (id == UINT32_MAX) throw std::invalid_argument("Layer ID is too large");
                next = (std::max)(next, id + 1);
            }
            for (auto& node : record["nodes"])
            {
                if (!node.is_object()) throw std::invalid_argument("Each layer needs a text and parent ID");
                const uint32_t id = node.value("id", 0u) ? node["id"].get<uint32_t>() : next++;
                if (parents.count(id)) throw std::invalid_argument("Layer IDs must be unique");
                node["id"] = id;
                node["parent"] = node.value("parent", 0u);
                node["text"] = node.value("text", std::string("Layer"));
                node["expanded"] = node.value("expanded", true);
                node["disabled"] = node.value("disabled", false);
                parents[id] = node["parent"].get<uint32_t>();
            }
            for (const auto& entry : parents)
            {
                std::set<uint32_t> visited{entry.first};
                for (auto parent = entry.second; parent; parent = parents.at(parent))
                {
                    if (!parents.count(parent)) throw std::invalid_argument("Parent ID must identify another layer or be 0");
                    if (!visited.insert(parent).second) throw std::invalid_argument("Layer hierarchy cannot contain a cycle");
                }
            }
            if (!parents.count(record.value("selected", 0u))) record["selected"] = 0;
        }

        static bool ContainsDocument(lv_obj_t* object)
        {
            const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
            if (IsDocumentObject(object) || (data && data->toolTray)) return true;
            for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
                if (ContainsDocument(lv_obj_get_child(object, i))) return true;
            return false;
        }

        static bool Generated(const State& state, lv_obj_t* object)
        {
            for (const auto& part : state.parts) if (part.second == object) return true;
            for (const auto& row : state.rows) if (row.second.button == object) return true;
            return false;
        }

        static json Children(lv_obj_t* parent, const State& state)
        {
            auto children = json::array();
            for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i)
            {
                auto* child = lv_obj_get_child(parent, i);
                const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(child));
                if (Generated(state, child) || IsDocumentObject(child) || (data && data->toolTray)) continue;
                auto record = ObjectSerializer::SerializeObject(child);
                if (!Type(child) && (lv_obj_get_class(child) == &lv_obj_class || lv_obj_has_class(child, &lv_button_class)))
                    record["contentChildren"] = ExtendedWidgets::Children(child);
                children.push_back(std::move(record));
            }
            return children;
        }

        static void ApplyLayer(lv_obj_t* root, State& state, const json& old)
        {
            auto& record = state.record;
            auto* content = state.parts.at("Content");
            lv_obj_set_hidden(state.parts.at("Title"), !record.value("showTitle", true));
            std::set<uint32_t> retained;
            std::map<uint32_t, const json*> nodes;
            std::map<uint32_t, std::vector<uint32_t>> children;
            for (const auto& node : record["nodes"])
            {
                const auto id = node.at("id").get<uint32_t>();
                nodes[id] = &node;
                children[node.at("parent").get<uint32_t>()].push_back(id);
                retained.insert(id);
                auto found = state.rows.find(id);
                if (found == state.rows.end())
                {
                    auto* button = lv_button_create(content);
                    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
                    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
                    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                    lv_obj_set_style_pad_all(button, 4, LV_PART_MAIN);
                    lv_obj_set_scrollable(button, false);
                    auto* toggle = Button(button, LV_SYMBOL_RIGHT);
                    lv_obj_set_size(toggle, 20, 24);
                    auto* caption = lv_label_create(button);
                    lv_obj_set_flex_grow(caption, 1);
                    lv_label_set_long_mode(caption, LV_LABEL_LONG_MODE_DOTS);
                    state.rows.emplace(id, Row{button, toggle, caption});
                    Bind(button, root, 0, id);
                    Bind(toggle, root, 0, id, true);
                }
            }
            for (auto iterator = state.rows.begin(); iterator != state.rows.end();)
                if (!retained.count(iterator->first)) { lv_obj_delete(iterator->second.button); iterator = state.rows.erase(iterator); }
                else ++iterator;
            uint32_t position = 0;
            const auto place = [&](const auto& self, uint32_t parent, int depth, bool visible) -> void {
                for (auto id : children[parent])
                {
                    const auto& node = *nodes.at(id);
                    auto& row = state.rows.at(id);
                    lv_obj_set_width(row.button, LV_PCT(100));
                    lv_obj_set_height(row.button, record.at("rowHeight").get<int>());
                    lv_obj_set_style_pad_left(row.button, 4 + depth * record.at("indent").get<int>(), LV_PART_MAIN);
                    lv_obj_set_hidden(row.button, !visible);
                    lv_obj_set_state(row.button, LV_STATE_DISABLED, node.value("disabled", false));
                    lv_obj_set_state(row.toggle, LV_STATE_DISABLED, node.value("disabled", false));
                    lv_obj_set_state(row.button, LV_STATE_CHECKED, record.value("selected", 0u) == id);
                    lv_label_set_text(row.caption, node.at("text").get<std::string>().c_str());
                    lv_label_set_text(lv_obj_get_child(row.toggle, 0), children[id].empty() ? LV_SYMBOL_MINUS :
                        node.value("expanded", true) ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT);
                    lv_obj_set_clickable(row.toggle, !children[id].empty());
                    lv_obj_move_to_index(row.button, position++);
                    self(self, id, depth + 1, visible && node.value("expanded", true));
                }
            };
            place(place, 0, 0, true);
        }

    public:
        static const char* Type(lv_obj_t* object)
        {
            if (!object || lv_obj_get_class(object) != &lv_obj_class) return nullptr;
            const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
            if (!data) return nullptr;
            if (data->objectJson.contains("textBox")) return "textBox";
            if (data->objectJson.contains("layerView")) return "layerView";
            return nullptr;
        }

        static lv_obj_t* Create(const std::string& type, lv_obj_t* parent = nullptr)
        {
            if (type != "textBox" && type != "layerView") return nullptr;
            if ((type == "textBox" && !LVSERIAL_USE_TEXTBOX) || (type == "layerView" && !LVSERIAL_USE_LAYERVIEW)) return nullptr;
            auto* root = lv_obj_create(parent ? parent : lv_screen_active());
            LVObject::FromJSON(json::object(), root);
            PreserveSection(root, type.c_str(), json::object());
            auto& state = WidgetData<State>(root);
            state.record = Defaults(type);
            lv_obj_set_scrollable(root, false);
            lv_obj_set_style_pad_all(root, 6, LV_PART_MAIN);
            lv_obj_set_flex_flow(root, type == "textBox" ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            auto* title = lv_label_create(root);
            state.parts["Title"] = title;
#if LVSERIAL_USE_TEXTBOX
            if (type == "textBox")
            {
                lv_obj_set_size(root, 280, 54);
                auto* entry = lv_textarea_create(root);
                state.parts["Entry"] = entry;
                lv_obj_set_flex_grow(entry, 1);
                lv_obj_set_height(entry, 36);
                lv_textarea_set_one_line(entry, true);
                Bind(entry, root);
                state.parts["Minus"] = Button(root, LV_SYMBOL_MINUS);
                state.parts["Plus"] = Button(root, LV_SYMBOL_PLUS);
                Bind(state.parts["Minus"], root, -1);
                Bind(state.parts["Plus"], root, 1);
            }
            else
#endif
            {
                lv_obj_set_size(root, 260, 180);
                lv_obj_set_width(title, LV_PCT(100));
                auto* content = lv_obj_create(root);
                state.parts["Content"] = content;
                lv_obj_set_width(content, LV_PCT(100));
                lv_obj_set_flex_grow(content, 1);
                lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
                lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN);
                lv_obj_set_style_pad_row(content, 4, LV_PART_MAIN);
                lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
                lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
            }
            Apply(root, state.record);
            return root;
        }

        static lv_obj_t* Part(lv_obj_t* object, const std::string& name)
        {
            auto& parts = WidgetData<State>(object).parts;
            const auto found = parts.find(name);
            return found == parts.end() ? nullptr : found->second;
        }
        static lv_obj_t* RowObject(lv_obj_t* object, uint32_t id)
        {
            auto& rows = WidgetData<State>(object).rows;
            const auto found = rows.find(id);
            return found == rows.end() ? nullptr : found->second.button;
        }
        static lv_obj_t* ContentParent(lv_obj_t* object)
        { const auto* type = Type(object); return type && std::string(type) == "layerView" ? Part(object, "Content") : object; }

        static void Apply(lv_obj_t* root, const json& incoming)
        {
            const auto* type = Type(root);
            if (!type) throw std::invalid_argument("Unknown custom widget");
            auto& state = WidgetData<State>(root);
            const auto old = state.record;
            auto record = old;
            record.update(incoming);
            const bool textBox = std::string(type) == "textBox";
            if (textBox)
            {
                const auto low = record.at("min").get<int32_t>(), high = record.at("max").get<int32_t>();
                if (low > high || record.at("step").get<int32_t>() <= 0) throw std::invalid_argument("Use an ordered range and a positive step");
                if (record.at("titleWidth").get<int>() < 0 || record.at("buttonWidth").get<int>() < 16 || record.at("maxLength").get<int>() < 0)
                    throw std::invalid_argument("Use nonnegative title width and text length, and button width of at least 16");
                if (record.value("numeric", true))
                {
                    int64_t value = record.at("value").get<int32_t>();
                    bool parsed = false;
                    if (Changed(old, record, "text") && !Changed(old, record, "value"))
                    {
                        try
                        {
                            const auto text = record.at("text").get<std::string>();
                            size_t used = 0;
                            const auto typed = std::stoll(text, &used);
                            if (used == text.size()) { value = typed; parsed = true; }
                        }
                        catch (const std::exception&) { /* Incomplete numeric text is still saved. */ }
                    }
                    value = std::clamp<int64_t>(value, low, high);
                    record["value"] = value;
                    if (Changed(old, record, "value") || Changed(old, record, "numeric") || parsed)
                        record["text"] = std::to_string(value);
                }
            }
            else
            {
                if (record.at("rowHeight").get<int>() < 24 || record.at("indent").get<int>() < 0 || record.at("indent").get<int>() > 128)
                    throw std::invalid_argument("Row height must be at least 24 and indent must be between 0 and 128");
                ValidateNodes(record);
            }
            struct Updating { bool& flag; Updating(bool& value) : flag(value) { flag = true; } ~Updating() { flag = false; } } updating(state.updating);
            state.record = record;
            if (record.contains("partBases") && Changed(old, record, "partBases"))
                for (const auto& part : record["partBases"].items())
                    if (auto* object = Part(root, part.key())) LVObject::FromJSON(part.value(), object);
            if (record.contains("styles"))
                for (const auto& part : record["styles"].items())
                    if (auto* object = Part(root, part.key()))
                        if (!old.contains("styles") || !old["styles"].contains(part.key()) || old["styles"][part.key()] != part.value())
                            Style::Apply(object, part.value(), LV_PART_MAIN);
            lv_label_set_text(state.parts.at("Title"), record.at("title").get<std::string>().c_str());
#if LVSERIAL_USE_TEXTBOX
            if (textBox)
            {
                auto* entry = state.parts.at("Entry");
                lv_obj_set_width(state.parts.at("Title"), record.at("titleWidth").get<int>());
                lv_label_set_long_mode(state.parts.at("Title"), LV_LABEL_LONG_MODE_DOTS);
                lv_obj_set_style_pad_column(root, record.at("spacing").get<int>(), LV_PART_MAIN);
                lv_textarea_set_max_length(entry, record.at("maxLength").get<uint32_t>());
                lv_textarea_set_password_mode(entry, record.value("password", false));
                lv_textarea_set_placeholder_text(entry, record.at("placeholder").get<std::string>().c_str());
                lv_textarea_set_accepted_chars(entry, record.at("acceptedChars").get_ref<const std::string&>().empty() ? nullptr :
                    state.record.at("acceptedChars").get_ref<const std::string&>().c_str());
                lv_textarea_set_text(entry, record.at("text").get<std::string>().c_str());
                lv_obj_set_state(entry, LV_STATE_DISABLED, record.value("readOnly", false));
                for (const auto* name : {"Minus", "Plus"})
                {
                    auto* button = state.parts.at(name);
                    lv_obj_set_size(button, record.at("buttonWidth").get<int>(), 32);
                    lv_obj_set_hidden(button, !record.value("showButtons", true));
                    lv_obj_set_state(button, LV_STATE_DISABLED, record.value("readOnly", false) || !record.value("numeric", true));
                    lv_label_set_text(lv_obj_get_child(button, 0), record.at(std::string(name) == "Plus" ? "plusText" : "minusText").get<std::string>().c_str());
                }
            }
            else
#endif
                ApplyLayer(root, state, old);
            if (!textBox)
                for (const auto& node : record["nodes"])
                {
                    auto& row = state.rows.at(node.at("id").get<uint32_t>());
                    if (node.contains("base")) LVObject::FromJSON(node["base"], row.button);
                    if (node.contains("toggleBase")) LVObject::FromJSON(node["toggleBase"], row.toggle);
                    if (node.contains("captionBase")) LVObject::FromJSON(node["captionBase"], row.caption);
                    lv_obj_set_state(row.button, LV_STATE_CHECKED, record.value("selected", 0u) == node.at("id").get<uint32_t>());
                }
            if (!textBox) ApplyLayer(root, state, old);
            if (Changed(old, record, "children"))
            {
                auto* content = ContentParent(root);
                for (int32_t i = static_cast<int32_t>(lv_obj_get_child_count(content)) - 1; i >= 0; --i)
                {
                    auto* child = lv_obj_get_child(content, i);
                    if (!Generated(state, child) && !ContainsDocument(child)) lv_obj_delete(child);
                }
                ExtendedWidgets::RestoreChildren(content, record["children"]);
            }
            // Keep the model consistent with textarea filtering and maximum length.
#if LVSERIAL_USE_TEXTBOX
            if (textBox) state.record["text"] = lv_textarea_get_text(state.parts.at("Entry"));
#endif
            PreserveSection(root, type, state.record);
        }

        static json ToJSON(lv_obj_t* root)
        {
            const auto* type = Type(root);
            auto& state = WidgetData<State>(root);
            auto record = state.record;
#if LVSERIAL_USE_TEXTBOX
            if (std::string(type) == "textBox") record["text"] = lv_textarea_get_text(state.parts.at("Entry"));
#endif
            record["styles"] = json::object();
            record["partBases"] = json::object();
            for (const auto& part : state.parts)
            {
                record["styles"][part.first] = Style::Read(part.second, LV_PART_MAIN);
                record["partBases"][part.first] = LVObject::ToJSON(part.second);
            }
            if (std::string(type) == "layerView")
                for (auto& node : record["nodes"])
                {
                    auto& row = state.rows.at(node.at("id").get<uint32_t>());
                    node["base"] = LVObject::ToJSON(row.button);
                    node["toggleBase"] = LVObject::ToJSON(row.toggle);
                    node["captionBase"] = LVObject::ToJSON(row.caption);
                }
            record["children"] = Children(ContentParent(root), state);
            return {{"base", LVObject::ToJSON(root)}, {type, record}};
        }

        static lv_obj_t* FromJSON(const json& record)
        {
            for (const auto* type : {"textBox", "layerView"})
            {
                if (!record.contains(type)) continue;
                auto* root = Create(type);
                if(!root) throw std::invalid_argument(std::string("Widget disabled in this runtime: ")+type);
                try
                {
                    LVObject::FromJSON(record.at("base"), root);
                    Apply(root, record.at(type));
                    LVObject::FromJSON(record.at("base"), root);
                    return root;
                }
                catch (...) { lv_obj_delete(root); throw; }
            }
            return nullptr;
        }
    };
#else
    class CustomWidgets {
    public:
        static const char* Type(lv_obj_t*) { return nullptr; }
        static lv_obj_t* Create(const std::string&,lv_obj_t* = nullptr) { return nullptr; }
        static lv_obj_t* Part(lv_obj_t*,const std::string&) { return nullptr; }
        static lv_obj_t* RowObject(lv_obj_t*,uint32_t) { return nullptr; }
        static lv_obj_t* ContentParent(lv_obj_t* object) { return object; }
        static void Apply(lv_obj_t*,const json&) { throw std::invalid_argument("Custom widgets disabled in this runtime"); }
        static json ToJSON(lv_obj_t*) { return nullptr; }
        static lv_obj_t* FromJSON(const json& record) {
            if(record.contains("textBox") || record.contains("layerView")) throw std::invalid_argument("Custom widget disabled in this runtime");
            return nullptr;
        }
    };
#endif
}
#endif
