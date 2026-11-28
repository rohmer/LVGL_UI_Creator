#pragma once

#include "../PropertyWindow.h"
#include "../../Utils/EditorControls.h"
#include "../../Utils/EditorEdit.h"
#include "../../Utils/EditorStyle.h"
#include <charconv>
#include <cstdio>
#include <iomanip>
#include <memory>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#else
#include "../../Utils/NativeFileDialogUI.h"
#endif

#if LVGL_VERSION_MAJOR >= 9
// The project stores configuration; the runtime creates the keyboard only when
// a text field is edited. There is deliberately no keyboard proxy in Design.
class ProjectKeyboardProperties
{
    enum class Kind { Enabled, Language, Size, Trie, Boolean, Number, Color, Height, Load };
    struct Context { PropertyWindow* window; lv_obj_t* status = nullptr; };
    struct Binding {
        std::shared_ptr<Context> context;
        Kind kind;
        std::string key;
        std::vector<json> choices;
    };

    static lv_obj_t* Column(lv_obj_t* parent)
    {
        auto* content = lv_cont_create(parent, nullptr);
        lv_obj_set_size(content, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
        return content;
    }

    static lv_obj_t* Label(lv_obj_t* parent, const std::string& text)
    {
        auto* label = lv_label_create(parent);
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
        lv_label_set_text(label, text.c_str());
        return label;
    }

    static bool Current(const Context& context)
    {
        return !context.window->Drawing() && context.window->ShowingProjectProperties() &&
            context.window->projectKeyboardSettings && context.window->applyProjectKeyboardSettings;
    }

    static size_t ModelBytes(const json& record, const char* key)
    {
        const auto found = record.find(key);
        if(found == record.end() || !found->is_string())return 0;
        const auto& encoded = found->get_ref<const std::string&>();
        if(encoded.empty())return 0;
        return encoded.size() / 4 * 3 - (encoded.back() == '=' ? 1 : 0) -
            (encoded.size() > 1 && encoded[encoded.size() - 2] == '=' ? 1 : 0);
    }

    static std::filesystem::path PickTrie()
    {
#ifdef _WIN32
        wchar_t path[32768]{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof dialog;
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter = L"NGTR trie (*.ngtr;*.trie;*.bin)\0*.ngtr;*.trie;*.bin\0All files\0*.*\0\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = _countof(path);
        dialog.lpstrTitle = L"Load prediction and autocorrect trie";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
        return GetOpenFileNameW(&dialog) ? std::filesystem::path(path) : std::filesystem::path{};
#else
        return NativeFileDialogUI::PickOne(false, false, L"Load prediction and autocorrect trie",
            L"NGTR trie\0*.ngtr;*.trie;*.bin\0All files\0*.*\0\0");
#endif
    }

    static void Change(lv_event_t* event)
    {
        auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
        const auto code = lv_event_get_code(event);
        if(code == LV_EVENT_DELETE) { delete binding; return; }
        const auto context = binding->context;
        if(!Current(*context))return;
        auto* control = lv_event_get_current_target_obj(event);
        if(code == LV_EVENT_CLICKED && binding->kind != Kind::Load) {
            FocusEditorControl(context->window->GetKBGroup(), control);
            return;
        }
        if(code != (binding->kind == Kind::Load ? LV_EVENT_CLICKED : LV_EVENT_VALUE_CHANGED))return;
        try {
            std::filesystem::path path;
            if(binding->kind == Kind::Load) {
                path = PickTrie();
                if(path.empty() || !Current(*context))return;
            }
            const bool text = binding->kind == Kind::Number || binding->kind == Kind::Color || binding->kind == Kind::Height;
            EditorEdit::Scope edit(control, text);
            const bool imports = binding->kind == Kind::Language || binding->kind == Kind::Size || binding->kind == Kind::Load;
            const auto before = imports ? context->window->Tries().ToJSON() : json{};
            try {
                const auto previous = context->window->projectKeyboardSettings();
                json patch;
                if(binding->kind == Kind::Enabled || binding->kind == Kind::Boolean)
                    patch = {{binding->key, lv_obj_has_state(control, LV_STATE_CHECKED)}};
                else if(binding->kind == Kind::Load)
                    patch = {{"trieId", context->window->Tries().Import(path)}, {"trieSizeKiB", 0}};
                else if(binding->kind == Kind::Language || binding->kind == Kind::Size || binding->kind == Kind::Trie) {
                    const auto index = lv_dropdown_get_selected(control);
                    if(index >= binding->choices.size())return;
                    const auto& choice = binding->choices[index];
                    if(binding->kind == Kind::Language)
                        patch = context->window->Tries().LanguageSettings(previous, choice.get<std::string>());
                    else if(binding->kind == Kind::Trie)
                        patch = {{"trieId", choice}, {"trieSizeKiB", 0}};
                    else if(choice.at("sizeKiB").get<uint32_t>() && choice.at("id").get<std::string>().empty())
                        patch = context->window->Tries().BundledSettings(previous.at("language").get<std::string>(), choice.at("sizeKiB").get<uint32_t>());
                    else patch = {{"trieId", choice.at("id")}, {"trieSizeKiB", choice.at("sizeKiB")}};
                }
                else {
                    const std::string value = lv_textarea_get_text(control);
                    if(binding->kind == Kind::Height && value.empty())patch = {{"height", nullptr}};
                    else if(binding->kind == Kind::Color) {
                        uint32_t color = 0;
                        if(value.size() != 7 || value.front() != '#')throw std::invalid_argument("Use #RRGGBB for a color");
                        const auto parsed = std::from_chars(value.data() + 1, value.data() + value.size(), color, 16);
                        if(parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())throw std::invalid_argument("Use #RRGGBB for a color");
                        patch = {{binding->key, 0xff000000u | color}};
                    }
                    else {
                        auto number = json::parse(value);
                        if(!number.is_number())throw std::invalid_argument("Enter a number");
                        patch = {{binding->key, std::move(number)}};
                    }
                }
                context->window->applyProjectKeyboardSettings(patch);
            }
            catch(...) {
                if(imports && context->window->Tries().ToJSON() != before)context->window->Tries().Load(before);
                throw;
            }
            lv_obj_set_style_border_color(control, lv_color_hex(EditorStyle::Border), LV_PART_MAIN);
            if(context->status)lv_label_set_text(context->status, "");
            if(!text && binding->kind != Kind::Boolean)context->window->RefreshProjectPropertiesAsync();
        }
        catch(const std::exception& error) {
            lv_obj_set_style_border_color(control, lv_color_hex(EditorStyle::Error), LV_PART_MAIN);
            if(context->status)lv_label_set_text(context->status, error.what());
        }
    }

    static void Bind(const std::shared_ptr<Context>& context, lv_obj_t* control, Kind kind,
        const std::string& key, std::vector<json> choices = {})
    {
        lv_obj_add_event_cb(control, Change, LV_EVENT_ALL, new Binding{context, kind, key, std::move(choices)});
    }

    static void Boolean(const std::shared_ptr<Context>& context, lv_obj_t* parent, const char* title,
        const char* key, bool checked, Kind kind = Kind::Boolean)
    {
        auto* checkbox = lv_checkbox_create(parent);
        lv_checkbox_set_text(checkbox, title);
        lv_obj_set_state(checkbox, LV_STATE_CHECKED, checked);
        Bind(context, checkbox, kind, key);
    }

    static void Dropdown(const std::shared_ptr<Context>& context, lv_obj_t* parent, const char* title,
        const std::string& options, uint32_t selected, Kind kind, std::vector<json> choices)
    {
        auto* field = Column(parent);
        Label(field, title);
        auto* selector = lv_dropdown_create(field);
        lv_obj_set_width(selector, LV_PCT(100));
        lv_dropdown_set_options(selector, options.c_str());
        lv_dropdown_set_selected(selector, selected);
        Bind(context, selector, kind, {}, std::move(choices));
    }

    static void Number(const std::shared_ptr<Context>& context, lv_obj_t* parent, const char* title,
        const char* key, const json& value, Kind kind = Kind::Number)
    {
        auto* field = Column(parent);
        Label(field, title);
        auto* input = lv_textarea_create(field);
        lv_textarea_set_one_line(input, true);
        lv_obj_set_size(input, LV_PCT(100), 36);
        std::string text = value.is_null() ? "" : value.dump();
        if(kind == Kind::Color) {
            char color[8];std::snprintf(color, sizeof color, "#%06x", value.get<uint32_t>() & 0xffffffu);
            text = color;
        }
        lv_textarea_set_text(input, text.c_str());
        lv_textarea_set_accepted_chars(input, kind == Kind::Color ? "#0123456789abcdefABCDEF" : "0123456789.");
        Bind(context, input, kind, key);
    }

public:
    static void LoadTrie(PropertyWindow* window, const std::filesystem::path& path)
    {
        if(!window->ShowingProjectProperties() || !window->applyProjectKeyboardSettings)
            throw std::invalid_argument("Select Project first");
        EditorEdit::Scope edit;
        const auto before = window->Tries().ToJSON();
        try {
            const auto id = window->Tries().Import(path);
            window->applyProjectKeyboardSettings({{"trieId", id}, {"trieSizeKiB", 0}});
        }
        catch(...) { if(window->Tries().ToJSON() != before)window->Tries().Load(before);throw; }
        window->RefreshProjectPropertiesAsync();
    }

    static void Update(PropertyWindow* window)
    {
        window->Drawing(true);
        auto* panel = window->ObjectPropWin();
        panel->DeleteChildren();
        panel->SetVisible(true);
        if(panel->IsCollapsed())panel->ToggleCollapsed();
        panel->UpdateHeight(LV_SIZE_CONTENT);
        auto* content = Column(panel->GetWindow());
        panel->AddObjectToWindow(content);
        Label(content, "Project Advanced Keyboard");
        auto context = std::make_shared<Context>(Context{window});
        const auto settings = window->projectKeyboardSettings();
        Boolean(context, content, "Enable Advanced Keyboard", "enabled", settings.at("enabled").get<bool>(), Kind::Enabled);
        Label(content, "The keyboard stays hidden in Design. Run (F6), then select a text box to type. These settings apply to every screen in this project.");
        if(settings.at("enabled").get<bool>()) {
#if LVSERIAL_USE_ADVANCED_KEYBOARD
            std::string names;std::vector<json> languages;uint32_t selected = 0;
            for(size_t index = 0; index < lv_advanced_keyboard_language_count(); ++index) {
                if(index)names += '\n';
                names += lv_advanced_keyboard_language_name(index);
                languages.emplace_back(lv_advanced_keyboard_language_id(index));
                if(languages.back() == settings.at("language"))selected = static_cast<uint32_t>(index);
            }
            Dropdown(context, content, "Language", names, selected, Kind::Language, std::move(languages));
#endif
            const auto language = settings.at("language").get<std::string>();
            const auto id = settings.at("trieId").get<std::string>();
            const auto tier = settings.at("trieSizeKiB").get<uint32_t>();
            std::string sizeNames = "None";uint32_t sizeIndex = 0;
            std::vector<json> sizes = {{{"sizeKiB", 0}, {"id", ""}}};
            if(language == "en-US" || id == "builtin-en-us") {
                sizeNames += "\nBuilt-in English";
                sizes.push_back({{"sizeKiB", 0}, {"id", "builtin-en-us"}});
                if(id == "builtin-en-us")sizeIndex = static_cast<uint32_t>(sizes.size() - 1);
            }
            for(const auto& model : BundledTries::Models())if(model.language == language) {
                sizeNames += '\n';sizeNames += std::to_string(model.sizeKiB) + " KiB" + (model.sizeKiB == 16 ? " (small)" : " (large)");
                sizes.push_back({{"sizeKiB", model.sizeKiB}, {"id", ""}});
                if(tier == model.sizeKiB && !id.empty())sizeIndex = static_cast<uint32_t>(sizes.size() - 1);
            }
            if(!id.empty() && sizeIndex == 0) {
                sizeNames += tier ? "\n" + std::to_string(tier) + " KiB (embedded)" : "\nCustom / project trie";
                sizes.push_back({{"sizeKiB", tier}, {"id", id}});sizeIndex = static_cast<uint32_t>(sizes.size() - 1);
            }
            Dropdown(context, content, "Trie size", sizeNames, sizeIndex, Kind::Size, std::move(sizes));
            std::string trieNames = "None\nEnglish (built-in)";uint32_t trieIndex = 0;
            std::vector<json> tries = {"", "builtin-en-us"};
            for(const auto& record : window->Tries().ToJSON()) {
                trieNames += '\n';trieNames += record.at("name").get<std::string>();tries.push_back(record.at("id"));
            }
            for(size_t index = 0; index < tries.size(); ++index)if(tries[index] == id)trieIndex = static_cast<uint32_t>(index);
            Dropdown(context, content, "Prediction trie", trieNames, trieIndex, Kind::Trie, std::move(tries));
            auto* load = lv_button_create(content);lv_obj_set_size(load, LV_PCT(100), 36);
            auto* caption = lv_label_create(load);lv_label_set_text(caption, "Load trie...");lv_obj_center(caption);
            Bind(context, load, Kind::Load, {});
            if(id.empty())Label(content, "Suggestions are off. Select a bundled trie size or load a custom trie.");
            else if(id == "builtin-en-us")Label(content, "Using the built-in English dictionary. Choose a corpus size to embed its model in this project.");
            else {
                bool found = false;
                for(const auto& record : window->Tries().ToJSON())if(record.at("id") == id) {
                    std::ostringstream description;
                    description << "Embedded model: " << record.at("name").get<std::string>() << ". Model " <<
                        std::fixed << std::setprecision(1) << double(ModelBytes(record, "data")) / 1024.0 <<
                        " KiB; index " << double(ModelBytes(record, "indexData")) / 1024.0 << " KiB.";
                    Label(content, description.str());found = true;break;
                }
                if(!found)Label(content, "The selected trie is unavailable. Load it again before exporting.");
            }
            if(!BundledTries::Find(language, 16) && !BundledTries::Find(language, 256))
                Label(content, "No bundled model is available for this language. Load a custom trie for suggestions.");
            Boolean(context, content, "Autocorrect", "autocorrect", settings.at("autocorrect").get<bool>());
            Number(context, content, "Autocorrect threshold (0-1)", "autocorrectThreshold", settings.at("autocorrectThreshold"));
            Boolean(context, content, "Right-to-left display", "rightToLeft", settings.at("rightToLeft").get<bool>());
            Number(context, content, "Key background", "keyBackground", settings.at("keyBackground"), Kind::Color);
            Number(context, content, "Key foreground", "keyForeground", settings.at("keyForeground"), Kind::Color);
            Number(context, content, "Key radius", "keyRadius", settings.at("keyRadius"));
            Number(context, content, "Keyboard height (px; blank = automatic)", "height", settings.value("height", json{}), Kind::Height);
        }
        context->status = Label(content, "");
        lv_obj_set_style_text_color(context->status, lv_color_hex(EditorStyle::Error), LV_PART_MAIN);
        window->CurrentlyLoadedProp = PropertyWindow::NONE;
        window->Drawing(false);
    }
};
#endif
