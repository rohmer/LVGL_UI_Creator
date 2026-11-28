#pragma once
#include "LVObject.h"
#include "AdvancedKeyboardSettings.h"
#include "RuntimeFeatures.h"
#include "WidgetRecords.h"
#include "ObjectSerializer.h"
#include "CustomWidgets.h"
#include "ProjectTries.h"
#include <cmath>
#include <map>
#include <vector>
#include <stdexcept>
#include <string>

#if LVSERIAL_USE_ADVANCED_KEYBOARD
#include <lv_advanced_keyboard.h>
#endif

namespace Serialization
{
// Persist the keyboard as one widget; external fields and shared trie assets
// retain their own records instead of duplicating private key controls.
class AdvancedKeyboard
{
public:
    static json Normalize(const json& input)
    { return NormalizeAdvancedKeyboardSettings(input); }

    static const char* Type(lv_obj_t* object)
    {
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        if (lv_advanced_keyboard_is_instance(object)) return "advancedKeyboard";
#else
        (void)object;
#endif
        return nullptr;
    }

    static lv_obj_t* Create(lv_obj_t* parent)
    {
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        auto* object = lv_advanced_keyboard_create(parent);
        if (object) LVObject::FromJSON(json::object(), object);
        return object;
#else
        (void)parent;
        return nullptr;
#endif
    }

    static void Apply(lv_obj_t* object, const json& input)
    {
        if (!Type(object)) throw std::invalid_argument("Advanced Keyboard requires LVGL 9.6 and LVSERIAL_USE_ADVANCED_KEYBOARD");
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        if (!input.is_object()) throw std::invalid_argument("Advanced Keyboard properties must be an object");
        const auto previous = Settings(object);
        auto merged = previous;
        merged.update(input);
        if (input.contains("language") && input.at("language") != previous.at("language") && !input.contains("rightToLeft"))
            merged.erase("rightToLeft");
        const auto next = Normalize(merged); // Validate every field before changing the live widget.
        auto changed = [&](const char* key) { return previous.at(key) != next.at(key); };
        lv_advanced_keyboard_set_hide_on_defocus(object, false);
        if (changed("trieId") || (!next.at("trieId").get_ref<const std::string&>().empty() && !lv_advanced_keyboard_get_predictor(object)))
            ProjectTries::Attach(object, next.at("trieId").get<std::string>(), next.at("autocorrect").get<bool>(), next.at("autocorrectThreshold").get<double>());
        if (changed("language") && !lv_advanced_keyboard_set_language(object, next.at("language").get_ref<const std::string&>().c_str()))
            throw std::invalid_argument("Could not select Advanced Keyboard language");
        lv_advanced_keyboard_set_right_to_left(object, next.at("rightToLeft").get<bool>());
        lv_advanced_keyboard_set_autocorrect(object, next.at("autocorrect").get<bool>());
        lv_advanced_keyboard_set_autocorrect_threshold(object, next.at("autocorrectThreshold").get<double>());
        if (changed("keyBackground")) lv_advanced_keyboard_set_key_background(object, lv_color_hex(next.at("keyBackground").get<uint32_t>()));
        if (changed("keyForeground")) lv_advanced_keyboard_set_key_foreground(object, lv_color_hex(next.at("keyForeground").get<uint32_t>()));
        if (changed("keyRadius")) lv_advanced_keyboard_set_key_radius(object, next.at("keyRadius").get<int32_t>());
        PreserveSection(object, "advancedKeyboard", next);
#else
        (void)input;
#endif
    }

    static json ToJSON(lv_obj_t* object)
    {
        if (!Type(object)) throw std::invalid_argument("Object is not an Advanced Keyboard");
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        lv_obj_update_layout(object);
        auto base = LVObject::ToJSON(object);
        base["hidden"] = AuthoredHidden(object);
        base["flags"]["hidden"] = AuthoredHidden(object);
        json record = {{"base", std::move(base)}, {"advancedKeyboard", Settings(object)}};
        const auto children = Children(object, 2);
        if (!children.empty()) record["contentChildren"] = children;
        return record;
#else
        return json::object();
#endif
    }

    static lv_obj_t* FromJSON(const json& record)
    {
        if (!record.contains("advancedKeyboard")) return nullptr;
        const auto properties = Normalize(record.at("advancedKeyboard"));
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        auto* object = Create(lv_screen_active());
        if (!object) throw std::invalid_argument("Advanced Keyboard requires LVGL 9.6 and LVSERIAL_USE_ADVANCED_KEYBOARD");
        try {
            LVObject::FromJSON(record.value("base", json::object()), object);
            Apply(object, properties);
            if (record.contains("contentChildren")) RestoreChildren(object, record.at("contentChildren"));
            lv_obj_set_hidden(object, true);
            return object;
        }
        catch (...) { lv_obj_delete(object); throw; }
#else
        (void)properties;
        throw std::invalid_argument("Advanced Keyboard requires LVGL 9.6 and LVSERIAL_USE_ADVANCED_KEYBOARD");
#endif
    }

    // Only authored content is searched; a Creator canvas can share an LVGL
    // screen with inspector text fields which must never become typing targets.
    static void BindTargets(lv_obj_t* root, const std::map<int, lv_obj_t*>& objects)
    {
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        for (const auto& item : objects) {
            auto* keyboard = item.second;
            if (!Type(keyboard)) continue;
            auto* active = lv_advanced_keyboard_get_textarea(keyboard);
            if (AuthoredHidden(keyboard)) {
                if (active || lv_advanced_keyboard_bound_textarea_count(keyboard)) lv_advanced_keyboard_unbind_all(keyboard);
                continue;
            }
            const auto settings = Settings(keyboard);
            const auto targetId = settings.at("targetOid").get<int>();
            std::vector<lv_obj_t*> fields;
            if (targetId >= 0) {
                const auto found = objects.find(targetId);
                auto* field = found == objects.end() ? nullptr : Target(found->second);
                if (!field || !Within(field, root)) throw std::invalid_argument("Advanced Keyboard targetOid must reference a textarea or Text box on its screen");
                fields.push_back(field);
            }
            else if (settings.at("autoFocus").get<bool>())
                for (const auto& authored : objects) if (Within(authored.second, root)) Collect(authored.second, fields);
            for (size_t index = lv_advanced_keyboard_bound_textarea_count(keyboard); index > 0; --index) {
                auto* old = lv_advanced_keyboard_bound_textarea(keyboard, index - 1);
                if (std::find(fields.begin(), fields.end(), old) == fields.end()) lv_advanced_keyboard_unbind_textarea(keyboard, old);
            }
            for (auto* field : fields)
                if (!lv_advanced_keyboard_bind_textarea(keyboard, field)) throw std::runtime_error("Could not bind Advanced Keyboard target");
            active = lv_advanced_keyboard_get_textarea(keyboard);
            if (!active) lv_obj_set_hidden(keyboard, true);
            if (lv_advanced_keyboard_is_dismissed(keyboard)) continue;
            if (targetId >= 0 && !settings.at("autoFocus").get<bool>() && active != fields.front())
                lv_advanced_keyboard_set_textarea(keyboard, fields.front());
            else if (!active) for (auto* field : fields) if (lv_obj_has_state(field, LV_STATE_FOCUSED)) {
                lv_advanced_keyboard_set_textarea(keyboard, field); break;
            }
        }
#else
        (void)root; (void)objects;
#endif
    }
    static void BindTargets(lv_obj_t* root, const std::vector<lv_obj_t*>& objects)
    {
        std::map<int, lv_obj_t*> indexed;
        for (size_t index = 0; index < objects.size(); ++index) indexed.emplace(static_cast<int>(index), objects[index]);
        BindTargets(root, indexed);
    }

private:
#if LVSERIAL_USE_ADVANCED_KEYBOARD
    static json Settings(lv_obj_t* object)
    {
        auto settings = Normalize(ReadSection(object, "advancedKeyboard"));
        settings["language"] = lv_advanced_keyboard_get_language(object);
        settings["rightToLeft"] = lv_advanced_keyboard_get_right_to_left(object);
        settings["hideOnDefocus"] = false;
        settings["autocorrect"] = lv_advanced_keyboard_get_autocorrect(object);
        settings["autocorrectThreshold"] = lv_advanced_keyboard_get_autocorrect_threshold(object);
        settings["keyBackground"] = 0xff000000u | lv_color_to_int(lv_advanced_keyboard_get_key_background(object));
        settings["keyForeground"] = 0xff000000u | lv_color_to_int(lv_advanced_keyboard_get_key_foreground(object));
        settings["keyRadius"] = lv_advanced_keyboard_get_key_radius(object);
        return settings;
    }

    static bool AuthoredHidden(lv_obj_t* object)
    {
        const auto base = ReadSection(object, "base");
        const auto hidden = base.find("hidden");
        if (hidden != base.end()) {
            if (hidden->is_boolean()) return hidden->get<bool>();
            if (hidden->is_number_integer()) return hidden->get<int64_t>() != 0;
        }
        const auto flags = base.find("flags");
        return flags != base.end() && flags->is_object() && flags->value("hidden", false);
    }
    static lv_obj_t* Target(lv_obj_t* object)
    {
        if (!object) return nullptr;
        if (lv_obj_has_class(object, &lv_textarea_class)) return object;
        const auto* type = CustomWidgets::Type(object);
        return type && std::string(type) == "textBox" ? CustomWidgets::Part(object, "Entry") : nullptr;
    }
    static void Collect(lv_obj_t* root, std::vector<lv_obj_t*>& fields)
    {
        if (!root) return;
        if (lv_obj_has_class(root, &lv_textarea_class) && std::find(fields.begin(), fields.end(), root) == fields.end()) fields.push_back(root);
        for (uint32_t index = 0; index < lv_obj_get_child_count(root); ++index) Collect(lv_obj_get_child(root, index), fields);
    }
    static bool Within(lv_obj_t* object, lv_obj_t* root)
    {
        for (auto* parent = object; parent; parent = lv_obj_get_parent(parent)) if (parent == root) return true;
        return false;
    }

    static json Children(lv_obj_t* parent, uint32_t first = 0)
    {
        auto children = json::array();
        for (uint32_t index = first; index < lv_obj_get_child_count(parent); ++index) {
            auto* child = lv_obj_get_child(parent, index);
            const auto* metadata = static_cast<const ObjectUserData*>(lv_obj_get_user_data(child));
            if (IsDocumentObject(child) || (metadata && metadata->toolTray)) continue;
            auto record = ObjectSerializer::SerializeObject(child);
            if (!Type(child) && !record.contains("contentChildren") && (lv_obj_get_class(child) == &lv_obj_class || lv_obj_get_class(child) == &lv_button_class)) {
                const auto nested = Children(child);
                if (!nested.empty()) record["contentChildren"] = nested;
            }
            children.push_back(std::move(record));
        }
        return children;
    }

    static void RestoreChildren(lv_obj_t* parent, const json& children)
    {
        if (!children.is_array()) throw std::invalid_argument("Advanced Keyboard contentChildren must be an array");
        for (const auto& record : children) {
            auto* child = ObjectSerializer::DeserializeObject(record);
            if (!child) throw std::invalid_argument("Unsupported Advanced Keyboard child widget");
            lv_obj_set_parent(child, parent);
            // Typed compositions restore their own content. Plain containers
            // use the shared contentChildren convention for nested objects.
            if (record.contains("contentChildren") && (lv_obj_get_class(child) == &lv_obj_class || lv_obj_get_class(child) == &lv_button_class) && !Type(child))
                RestoreChildren(child, record.at("contentChildren"));
        }
    }
#endif
};
}
