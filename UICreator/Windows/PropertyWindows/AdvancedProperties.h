#pragma once
#include "../../Utils/EditorStyle.h"
#ifndef _WIN32
#include "../../Utils/NativeFileDialogUI.h"
#endif

#include "../PropertyWindow.h"
#include "../../Utils/EditorControls.h"
#include <Serialization/ExtendedWidgets.h>
#include <Serialization/CustomWidgets.h>
#include <Serialization/LVCanvas.h>
#include <Serialization/LVCalendar.h>
#include <Serialization/CodeWidgets.h>
#include <Serialization/AdvancedKeyboard.h>
#include <set>
#include <sstream>
#include <iomanip>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#if LVGL_VERSION_MAJOR >= 9
class AdvancedProperties
{
    enum class Kind { Value, Numbers, Source, Width, Flag, Add, Remove };
    struct EditorState { std::set<std::string> expanded; };
    struct Binding
    {
        PropertyWindow* window;
        lv_obj_t* object;
        std::string path;
        Kind kind;
        json original;
        uint32_t mask = 0;
        std::vector<int> choices;
    };
    struct Rebuild { PropertyWindow* window; lv_obj_t* object; };

    static void Refresh(PropertyWindow* window, lv_obj_t* object)
    {
        lv_async_call([](void* data) {
            auto* request = static_cast<Rebuild*>(data);
            if (request->window->GetSelectedObject() == request->object)
                request->window->SetSelectedObject(request->object);
            delete request;
        }, new Rebuild{window, object});
    }

    static void Apply(PropertyWindow* window,lv_obj_t* object, const json& record)
    {
        using namespace Serialization;
        if (const auto* type = AdvancedKeyboard::Type(object)) window->Tries().ApplySettings(object,record.at(type));
        else if (const auto* type = CodeWidgets::Type(object)) CodeWidgets::Apply(object, record.at(type));
        else if (const auto* type = CustomWidgets::Type(object)) CustomWidgets::Apply(object, record.at(type));
        else if (const auto* type = ExtendedWidgets::Type(object))
        {
            const auto width = lv_obj_get_style_width(object, LV_PART_MAIN);
            const auto height = lv_obj_get_style_height(object, LV_PART_MAIN);
            ExtendedWidgets::Apply(object, record.at(type));
            if (std::string(type) == "imagebutton") lv_obj_set_size(object, width, height);
        }
        else if (record.contains("cal")) LVCalendar::Apply(object, record.at("cal"));
        else if (record.contains("canvas")) LVCanvas::Apply(object, record.at("canvas"));
        else if (record.contains("btnm")) LVButtonMatrix::Apply(object, record.at("btnm"));
    }

    static std::string Name(std::string key)
    {
        if (key == "hlDates") return "Highlighted dates";
        if (key == "dayN") return "Day names";
        if (key == "monthN") return "Month names";
        if (key == "btnMap") return "Button map";
        if (key == "nativeCtrl") return "Button controls";
        if (key == "oneToggle") return "One checked button";
        if (key == "children" || key == "contentChildren") return "Content";
        if (key == "headerButtons") return "Header buttons";
        if (key == "footer") return "Footer buttons";
        std::string result;
        for (char ch : key)
        {
            if (std::isupper(static_cast<unsigned char>(ch))) result += ' ';
            result += ch == '_' || ch == '.' ? ' ' : ch;
        }
        if (!result.empty()) result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
        return result;
    }

    static bool Color(const std::string& key)
    {
        return key == "color" || key == "keyBackground" || key == "keyForeground" || key.find("Color") != std::string::npos || key.find("_color") != std::string::npos;
    }

    static void Event(lv_event_t* event)
    {
        auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
        if (lv_event_get_code(event) == LV_EVENT_DELETE) { delete binding; return; }
        auto* field = lv_event_get_target_obj(event);
        if (binding->window->Drawing() || binding->window->GetSelectedObject() != binding->object) return;
        if (lv_event_get_code(event) == LV_EVENT_CLICKED && binding->kind != Kind::Add && binding->kind != Kind::Remove)
        {
            FocusEditorControl(binding->window->GetKBGroup(), field);
            return;
        }
        const bool action = binding->kind == Kind::Add || binding->kind == Kind::Remove;
        if (lv_event_get_code(event) != (action ? LV_EVENT_CLICKED : LV_EVENT_VALUE_CHANGED)) return;
        EditorEdit::Scope edit(lv_event_get_target_obj(event),lv_obj_get_class(lv_event_get_target_obj(event))==&lv_textarea_class);
        try
        {
            auto record = Serialization::ObjectSerializer::SerializeObject(binding->object);
            auto pointer = json::json_pointer(binding->path);
            auto& destination = record[pointer];
            if (binding->kind == Kind::Add) destination.push_back(binding->original);
            else if (binding->kind == Kind::Remove)
            {
                const auto separator = binding->path.rfind('/');
                auto& array = record[json::json_pointer(binding->path.substr(0, separator))];
                const auto index = std::stoi(binding->path.substr(separator + 1));
                if (binding->path.rfind("/layerView/nodes/", 0) == 0)
                {
                    std::set<uint32_t> removed{array.at(index).at("id").get<uint32_t>()};
                    bool changed;
                    do {
                        changed = false;
                        for (const auto& node : array)
                            if (removed.count(node.value("parent", 0u)) && removed.insert(node.at("id").get<uint32_t>()).second) changed = true;
                    } while (changed);
                    auto retained = json::array();
                    for (const auto& node : array) if (!removed.count(node.at("id").get<uint32_t>())) retained.push_back(node);
                    array = std::move(retained);
                }
                else array.erase(array.begin() + index);
            }
            else
            {
                json value;
                if (!binding->choices.empty()) value = binding->choices.at(lv_dropdown_get_selected(field));
                else if (lv_obj_get_class(field) == &lv_checkbox_class) value = lv_obj_has_state(field, LV_STATE_CHECKED);
                else
                {
                    const std::string text = lv_textarea_get_text(field);
                    const auto key = binding->path.substr(binding->path.rfind('/') + 1);
                    if (binding->kind == Kind::Numbers)
                    {
                        value = json::parse("[" + text + "]");
                        for (const auto& number : value)
                            if (!number.is_number()) throw std::invalid_argument("Enter comma-separated numbers");
                    }
                    else if (binding->kind == Kind::Source)
                        value = text.empty() ? json(nullptr) : json{{"kind", "text"}, {"source", text}};
                    else if (binding->original.is_string()) value = text;
                    else if (Color(key))
                    {
                        if (text.size() != 7 || text[0] != '#') throw std::invalid_argument("Use #RRGGBB for a color");
                        size_t used = 0;
                        value = 0xff000000u | static_cast<uint32_t>(std::stoul(text.substr(1), &used, 16));
                        if (used != 6) throw std::invalid_argument("Use #RRGGBB for a color");
                    }
                    else
                    {
                        value = json::parse(text);
                        if (!value.is_number()) throw std::invalid_argument("Enter a number");
                    }
                }
                if (binding->kind == Kind::Flag)
                    destination = value.get<bool>() ? destination.get<uint32_t>() | binding->mask : destination.get<uint32_t>() & ~binding->mask;
                else if (binding->kind == Kind::Width)
                    destination = (destination.get<uint32_t>() & ~0xfu) | std::clamp(value.get<uint32_t>(), 1u, 15u);
                else destination = value;
            }
            Apply(binding->window,binding->object, record);
            auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(binding->object));
            data->objectJson = Serialization::ObjectSerializer::SerializeObject(binding->object);
            lv_obj_set_style_border_color(field, lv_color_hex(EditorStyle::Border), LV_PART_MAIN);
            if (action) Refresh(binding->window, binding->object);
        }
        catch (const std::exception& error)
        {
            lv_obj_set_style_border_color(field, lv_color_hex(EditorStyle::Error), LV_PART_MAIN);
            auto* content = lv_obj_get_child(binding->window->ObjectPropWin()->GetWindow(), 0);
            auto* label = content ? lv_obj_get_child(content, -1) : nullptr;
            if (label && lv_obj_get_class(label) == &lv_label_class) lv_label_set_text(label, error.what());
        }
    }

    static void Bind(lv_obj_t* control, PropertyWindow* window, const std::string& path, Kind kind,
                     const json& original, uint32_t mask = 0, std::vector<int> choices = {})
    {
        lv_obj_add_event_cb(control, Event, LV_EVENT_ALL,
            new Binding{window, window->GetSelectedObject(), path, kind, original, mask, std::move(choices)});
    }

    static lv_obj_t* Column(lv_obj_t* parent)
    {
        auto* column = lv_cont_create(parent, nullptr);
        lv_obj_set_size(column, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_opa(column, LV_OPA_TRANSP, LV_PART_MAIN);
        return column;
    }

    static lv_obj_t* Group(PropertyWindow* window, lv_obj_t* parent, const std::string& path, const std::string& title)
    {
        auto* wrapper = Column(parent);
        auto* button = lv_button_create(wrapper);
        lv_obj_set_size(button, LV_PCT(100), 30);
        lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x414141), LV_PART_MAIN);
        lv_obj_set_style_pad_all(button, 6, LV_PART_MAIN);
        auto* label = lv_label_create(button);
        lv_label_set_text(label, title.c_str());
        auto* content = Column(wrapper);
        auto& state = Serialization::WidgetData<EditorState>(window->GetSelectedObject());
        lv_obj_set_hidden(content, !state.expanded.count(path));
        struct Toggle { PropertyWindow* window; lv_obj_t* object; lv_obj_t* content; std::string path; };
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto* toggle = static_cast<Toggle*>(lv_event_get_user_data(event));
            if (lv_event_get_code(event) == LV_EVENT_DELETE) { delete toggle; return; }
            if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
            auto& expanded = Serialization::WidgetData<EditorState>(toggle->object).expanded;
            const bool open = !expanded.count(toggle->path);
            if (open) expanded.insert(toggle->path); else expanded.erase(toggle->path);
            lv_obj_set_hidden(toggle->content, !open);
            toggle->window->GetCWM()->Update();
        }, LV_EVENT_ALL, new Toggle{window, window->GetSelectedObject(), content, path});
        return content;
    }

    static void Action(PropertyWindow* window, lv_obj_t* parent, const char* title, const std::string& path, Kind kind, json value = nullptr)
    {
        auto* button = lv_button_create(parent);
        lv_obj_set_size(button, LV_PCT(100), 32);
        lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
        auto* label = lv_label_create(button);
        lv_label_set_text(label, title);
        lv_obj_center(label);
        Bind(button, window, path, kind, value);
    }

    static void Field(PropertyWindow* window, lv_obj_t* parent, const std::string& path, const std::string& title,
                      const json& value, Kind kind = Kind::Value, uint32_t mask = 0)
    {
        lv_obj_t* control;
        std::vector<int> choices;
        std::string options;
        if (path == "/chart/type") { options = "None\nLine\nBar\nScatter"; choices = {0, 1, 2, 3}; }
        else if (path == "/chart/updateMode") { options = "Shift\nCircular"; choices = {0, 1}; }
        else if (path == "/scale/mode") { options = "Horizontal top\nHorizontal bottom\nVertical left\nVertical right\nRound inner\nRound outer"; choices = {LV_SCALE_MODE_HORIZONTAL_TOP, LV_SCALE_MODE_HORIZONTAL_BOTTOM, LV_SCALE_MODE_VERTICAL_LEFT, LV_SCALE_MODE_VERTICAL_RIGHT, LV_SCALE_MODE_ROUND_INNER, LV_SCALE_MODE_ROUND_OUTER}; }
        else if (path == "/tabview/position") { options = "Top\nBottom\nLeft\nRight"; choices = {LV_DIR_TOP, LV_DIR_BOTTOM, LV_DIR_LEFT, LV_DIR_RIGHT}; }
        else if (path == "/page/scrollDirection" || path.find("/directions") != std::string::npos)
        { options = "None\nHorizontal\nVertical\nAll\nLeft\nRight\nTop\nBottom"; choices = {LV_DIR_NONE, LV_DIR_HOR, LV_DIR_VER, LV_DIR_ALL, LV_DIR_LEFT, LV_DIR_RIGHT, LV_DIR_TOP, LV_DIR_BOTTOM}; }
        else if (path == "/page/scrollbarMode") { options = "Off\nOn\nActive\nAutomatic"; choices = {0, 1, 2, 3}; }
        else if (path == "/canvas/format") { options = "RGB565\nRGB888\nXRGB8888\nARGB8888"; choices = {LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_RGB888, LV_COLOR_FORMAT_XRGB8888, LV_COLOR_FORMAT_ARGB8888}; }
        else if (path == "/keyboard/mode") { options = "Lowercase\nUppercase\nSpecial\nNumbers\nUser 1\nUser 2\nUser 3\nUser 4"; choices = {0, 1, 2, 3, 4, 5, 6, 7}; }
        else if (path == "/barcode/encoding") { options = "Code 128 GS1\nCode 128 raw"; choices = {0, 1}; }
        else if (path == "/barcode/direction") { options = "Horizontal\nVertical"; choices = {LV_DIR_HOR, LV_DIR_VER}; }
        if (value.is_boolean())
        {
            control = lv_checkbox_create(parent);
            lv_checkbox_set_text(control, title.c_str());
            lv_obj_set_state(control, LV_STATE_CHECKED, value.get<bool>());
        }
        else
        {
            auto* field = Column(parent);
            auto* label = lv_label_create(field);
            lv_label_set_text(label, title.c_str());
            if (!choices.empty())
            {
                control = lv_dropdown_create(field);
                lv_dropdown_set_options(control, options.c_str());
                const auto selected = std::find(choices.begin(), choices.end(), value.get<int>());
                if (selected != choices.end()) lv_dropdown_set_selected(control, static_cast<uint32_t>(selected - choices.begin()));
            }
            else
            {
                control = lv_textarea_create(field);
                std::string text;
                if (kind == Kind::Numbers)
                {
                    for (const auto& number : value) { if (!text.empty()) text += ", "; text += number.dump(); }
                }
                else if (value.is_string()) text = value.get<std::string>();
                else if (Color(path.substr(path.rfind('/') + 1)))
                {
                    std::ostringstream stream;
                    stream << '#' << std::hex << std::setfill('0') << std::setw(6) << (value.get<uint32_t>() & 0xffffff);
                    text = stream.str();
                }
                else text = value.dump();
                const bool multiline = kind == Kind::Numbers || text.find('\n') != std::string::npos;
                lv_textarea_set_one_line(control, !multiline);
                lv_textarea_set_text(control, text.c_str());
                lv_obj_set_height(control, multiline ? 76 : 36);
                lv_obj_set_style_pad_all(control, 8, LV_PART_MAIN);
            }
            lv_obj_set_width(control, LV_PCT(100));
            lv_obj_set_style_bg_color(control, lv_color_hex(EditorStyle::Input), LV_PART_MAIN);
            lv_obj_set_style_border_color(control, lv_color_hex(EditorStyle::Border), LV_PART_MAIN);
        }
        Bind(control, window, path, kind, value, mask, std::move(choices));
    }

    static json NewItem(const json& record, const std::string& key)
    {
        if (key == "cursors") return {{"color", 0xffeeeeeeu}, {"directions", LV_DIR_ALL}, {"positionSet", true}, {"x", 0}, {"y", 0}, {"series", -1}, {"point", 0}};
        if (key == "hlDates") return record.at("cal").at("today");
        if (key == "series") return {{"color", 0xff4488ccu}, {"axis", 0}, {"hidden", false}, {"start", 0},
            {"y", std::vector<int>(record.at("chart").value("pointCount", 5u), 0)}};
        if (key == "tabs" || key == "tiles")
        {
            const auto& entries = record.at(key == "tabs" ? "tabview" : "tileview").at(key);
            uint32_t id = 1;
            for (const auto& entry : entries) id = std::max(id, entry.value("id", 0u) + 1);
            if (key == "tabs") return {{"id", id}, {"name", "Tab " + std::to_string(id)}, {"children", json::array()}};
            return {{"id", id}, {"column", entries.size()}, {"row", 0}, {"directions", LV_DIR_ALL}, {"children", json::array()}};
        }
        if (key == "nodes" && record.contains("layerView"))
        {
            uint32_t id = 1;
            for (const auto& node : record.at("layerView").at("nodes")) id = std::max(id, node.value("id", 0u) + 1);
            return {{"id", id}, {"parent", 0}, {"text", "Layer " + std::to_string(id)}, {"expanded", true}, {"disabled", false}};
        }
        if (key == "rows")
        {
            auto row = json::array();
            for (size_t i = 0; i < record.at("table").at("columns").size(); ++i) row.push_back({{"text", ""}, {"flags", 0}});
            return row;
        }
        if (key == "btnMap") return "Button";
        const json label{{"base", {{"coords", {{"width", LV_SIZE_CONTENT}, {"height", LV_SIZE_CONTENT}}}}}, {"text", "Text"}, {"longmode", 0}, {"textalign", 0}};
        if (key == "headerButtons") return {{"base", {{"coords", {{"width", 36}, {"height", 36}}}}},
            {"contentClass", "messageboxHeaderButton"}, {"close", false}, {"contentChildren", json::array({label})}};
        if (key == "footer") return {{"base", {{"coords", {{"width", 80}, {"height", 36}}}}},
            {"contentClass", "messageboxFooterButton"}, {"contentChildren", json::array({label})}};
        return label;
    }

    struct ImageChoice {PropertyWindow* window;lv_obj_t* object;size_t state,side;};
    static void ImageDropdown(PropertyWindow* window,lv_obj_t* parent,lv_obj_t* object,size_t state,size_t side,const char* sideName,const json& source){
        auto* row=Column(parent);auto* label=lv_label_create(row);lv_label_set_text(label,(std::string(sideName)+" project image").c_str());
        auto* choice=lv_dropdown_create(row);lv_obj_set_width(choice,LV_PCT(100));std::string options="Custom / current image\nNone";uint32_t selected=source.is_null()?1:0,index=2;
        auto id=source.is_object()?source.value("imageId",std::string{}):std::string{};
        for(const auto& entry:window->Images().ToJSON()){options+='\n';options+=entry.at("name").get<std::string>();if(entry.at("id")==id)selected=index;++index;}
        lv_dropdown_set_options(choice,options.c_str());lv_dropdown_set_selected(choice,selected);
        lv_obj_add_event_cb(choice,[](lv_event_t* e){auto* b=static_cast<ImageChoice*>(lv_event_get_user_data(e));auto code=lv_event_get_code(e);if(code==LV_EVENT_DELETE){delete b;return;}
            if(code==LV_EVENT_CLICKED){FocusEditorControl(b->window->GetKBGroup(),lv_event_get_target_obj(e));return;}
            if(code!=LV_EVENT_VALUE_CHANGED||b->window->GetSelectedObject()!=b->object)return;
            try{auto index=lv_dropdown_get_selected(lv_event_get_target_obj(e));auto id=index<2?std::string{}:b->window->Images().ToJSON().at(index-2).at("id").get<std::string>();
                b->window->Images().ApplyButton(b->object,b->state,b->side,id,index==0);Refresh(b->window,b->object);
            }catch(const std::exception& error){spdlog::error("Image button selection: {}",error.what());}
        },LV_EVENT_ALL,new ImageChoice{window,object,state,side});
    }

    static void Build(PropertyWindow* window, lv_obj_t* parent, const std::string& path, const std::string& key,
                      const json& value, const json& record)
    {
        // Encoded image/canvas bytes stay hidden; code payload text is editable.
        if (key == "data" && path != "/barcode/data" && path != "/qrcode/data") return;
        if (key == "partBases" || key == "toggleBase" || key == "captionBase" || key == "base" || key == "barBase" || key == "buttonBase" || key == "headerBase" || key == "contentBase" || key == "footerBase" || key == "nativeStyles" ||
            key == "id" || key == "contentClass" || key == "variant" || key == "ctrl" || key == "formatVersion" ||
            key == "cf" || key == "stride" || key == "header.w" || key == "header.h" || key == "img.w" || key == "img.h") return;
        if (path == "/window/header") return;
        if (value.is_object())
        {
            auto* group = Group(window, parent, path, Name(key));
            for (const auto& child : value.items()) Build(window, group, path + "/" + child.key(), child.key(), child.value(), record);
        }
        else if (value.is_array())
        {
            auto* group = Group(window, parent, path, Name(key));
            if (key == "states")
            {
                const char* states[] = {"Released", "Pressed", "Disabled", "Checked", "Checked pressed", "Checked disabled"};
                const char* sides[] = {"Left", "Middle", "Right"};
                for (size_t state = 0; state < value.size() && state < 6; ++state)
                {
                    auto* stateGroup = Group(window, group, path + "/" + std::to_string(state), states[state]);
                    for (size_t side = 0; side < value[state].size() && side < 3; ++side)
                    {
                        const auto& source = value[state][side];
                        ImageDropdown(window,stateGroup,window->GetSelectedObject(),state,side,sides[side],source);
                        const auto text = source.is_object() && source.value("kind", std::string{}) == "text" ? source.value("source", std::string{}) : "";
                        if (source.is_object() && source.value("kind", std::string{}) != "text")
                        {
                            auto* status = lv_label_create(stateGroup);
                            lv_label_set_text_fmt(status, "%s: embedded image", sides[side]);
                        }
                        Field(window, stateGroup, path + "/" + std::to_string(state) + "/" + std::to_string(side),
                            std::string(sides[side]) + " source (LVGL file path)", text, Kind::Source);
                    }
                }
                return;
            }
            if (key == "nativeCtrl")
            {
                const std::pair<const char*, uint32_t> flags[] = {{"Hidden", 0x10}, {"No repeat", 0x20}, {"Disabled", 0x40},
                    {"Checkable", 0x80}, {"Checked", 0x100}, {"Click trigger", 0x200}, {"Popover", 0x400}, {"Recolor", 0x800},
                    {"Custom 1", 0x4000}, {"Custom 2", 0x8000}};
                for (size_t i = 0; i < value.size(); ++i)
                {
                    const auto itemPath = path + "/" + std::to_string(i);
                    auto* item = Group(window, group, itemPath, "Button " + std::to_string(i + 1));
                    const auto mask = value[i].get<uint32_t>();
                    Field(window, item, itemPath, "Relative width", mask & 0xf, Kind::Width);
                    for (const auto& flag : flags) Field(window, item, itemPath, flag.first, (mask & flag.second) != 0, Kind::Flag, flag.second);
                }
                return;
            }
            if ((!value.empty() && value.front().is_number()) || key == "columns" || key == "x" || key == "y")
            {
                Field(window, group, path, "Values (comma-separated)", value, Kind::Numbers);
                return;
            }
            const bool fixed = key == "dayN" || key == "monthN" || key == "axes" || key == "btnStyles" || key == "Cells";
            for (size_t index = 0; index < value.size(); ++index)
            {
                const auto itemPath = path + "/" + std::to_string(index);
                auto* item = Group(window, group, itemPath, key == "nodes" && value[index].is_object() ?
                    value[index].value("text", std::string("Layer")) + " (#" + std::to_string(value[index].value("id", 0u)) + ")" :
                    "Item " + std::to_string(index + 1));
                if (value[index].is_object())
                    for (const auto& field : value[index].items()) Build(window, item, itemPath + "/" + field.key(), field.key(), field.value(), record);
                else if (value[index].is_array()) Build(window, item, itemPath, "Cells", value[index], record);
                else if (value[index] == "\n")
                {
                    auto* label = lv_label_create(item);
                    lv_label_set_text(label, "Row break");
                }
                else Field(window, item, itemPath, "Text", value[index]);
                if (!fixed) Action(window, item, "Remove", itemPath, Kind::Remove);
            }
            if (!fixed)
            {
                Action(window, group, "Add", path, Kind::Add, NewItem(record, key));
                if (key == "btnMap") Action(window, group, "Add row break", path, Kind::Add, "\n");
            }
        }
        else if (!value.is_null()) Field(window, parent, path, Name(key), value);
    }

public:
    static void LoadTrie(PropertyWindow* window,const std::filesystem::path& path) {
        auto* object=window->GetSelectedObject();
        if(!Serialization::AdvancedKeyboard::Type(object))throw std::invalid_argument("Select Advanced Keyboard first");
        EditorEdit::Scope edit;
        const auto id=window->Tries().Import(path);window->Tries().Apply(object,id);
        window->SetSelectedObject(object);
    }

    static void KeyboardControls(PropertyWindow* window,lv_obj_t* parent,const json& settings) {
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        auto* languageField=Column(parent);
        auto* languageTitle=lv_label_create(languageField);lv_label_set_text(languageTitle,"Language");
        auto* language=lv_dropdown_create(languageField);lv_obj_set_width(language,LV_PCT(100));
        std::string languageNames;uint32_t languageIndex=0;
        for(size_t index=0;index<lv_advanced_keyboard_language_count();++index) {
            if(index)languageNames+='\n';
            languageNames+=lv_advanced_keyboard_language_name(index);
            if(settings.value("language",std::string("en-US"))==lv_advanced_keyboard_language_id(index))languageIndex=static_cast<uint32_t>(index);
        }
        lv_dropdown_set_options(language,languageNames.c_str());lv_dropdown_set_selected(language,languageIndex);
        struct Language {PropertyWindow* window;lv_obj_t* object;lv_obj_t* trie=nullptr;};
        auto* languageBinding=new Language{window,window->GetSelectedObject()};
        lv_obj_add_event_cb(language,[](lv_event_t* event) {
            auto* state=static_cast<Language*>(lv_event_get_user_data(event));
            const auto code=lv_event_get_code(event);
            if(code==LV_EVENT_DELETE){delete state;return;}
            auto* field=lv_event_get_target_obj(event);
            if(code==LV_EVENT_CLICKED){FocusEditorControl(state->window->GetKBGroup(),field);return;}
            if(code!=LV_EVENT_VALUE_CHANGED || state->window->Drawing() || state->window->GetSelectedObject()!=state->object)return;
            const auto* id=lv_advanced_keyboard_language_id(lv_dropdown_get_selected(field));if(!id)return;
            try {
                EditorEdit::Scope edit;
                const auto previous=Serialization::AdvancedKeyboard::ToJSON(state->object).at("advancedKeyboard");
                json update={{"language",id}};
                const bool clearEnglish=std::string(id)!="en-US" && previous.value("trieId",std::string{})=="builtin-en-us";
                if(clearEnglish)update["trieId"]="";
                state->window->Tries().ApplySettings(state->object,update);
                if(clearEnglish && state->trie)lv_dropdown_set_selected(state->trie,0);
                lv_obj_set_style_border_color(field,lv_color_hex(EditorStyle::Border),LV_PART_MAIN);
            }catch(const std::exception& error){lv_obj_set_style_border_color(field,lv_color_hex(EditorStyle::Error),LV_PART_MAIN);}
        },LV_EVENT_ALL,languageBinding);
#endif
        auto* trieField=Column(parent);
        auto* trieTitle=lv_label_create(trieField);lv_label_set_text(trieTitle,"Prediction trie");
        auto* selector=lv_dropdown_create(trieField);lv_obj_set_width(selector,LV_PCT(100));
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        languageBinding->trie=selector;
#endif
        struct Selection {PropertyWindow* window;lv_obj_t* object;std::vector<std::string> ids;};
        auto* selection=new Selection{window,window->GetSelectedObject(),{"","builtin-en-us"}};
        std::string names="None\nEnglish (ESPOS bundled)";uint32_t selected=0;
        const auto id=settings.value("trieId",std::string("builtin-en-us"));
        for(const auto& entry:window->Tries().ToJSON()) {
            selection->ids.push_back(entry.at("id"));names+='\n';names+=entry.at("name").get<std::string>();
        }
        for(size_t i=0;i<selection->ids.size();++i)if(selection->ids[i]==id)selected=static_cast<uint32_t>(i);
        lv_dropdown_set_options(selector,names.c_str());lv_dropdown_set_selected(selector,selected);
        lv_obj_add_event_cb(selector,[](lv_event_t* event) {
            auto* state=static_cast<Selection*>(lv_event_get_user_data(event));
            if(lv_event_get_code(event)==LV_EVENT_DELETE){delete state;return;}
            if(lv_event_get_code(event)==LV_EVENT_CLICKED){FocusEditorControl(state->window->GetKBGroup(),lv_event_get_target_obj(event));return;}
            if(lv_event_get_code(event)!=LV_EVENT_VALUE_CHANGED || state->window->Drawing())return;
            auto* field=lv_event_get_target_obj(event);
            try {
                const auto choice=lv_dropdown_get_selected(field);
                if(choice<state->ids.size())state->window->Tries().Apply(state->object,state->ids[choice]);
            } catch(const std::exception& error) {lv_obj_set_style_border_color(field,lv_color_hex(EditorStyle::Error),LV_PART_MAIN);}
        },LV_EVENT_ALL,selection);
        auto* button=lv_button_create(parent);lv_obj_set_size(button,LV_PCT(100),36);
        auto* caption=lv_label_create(button);lv_label_set_text(caption,"Load trie...");lv_obj_center(caption);
        auto* status=lv_label_create(parent);lv_obj_set_width(status,LV_PCT(100));lv_label_set_long_mode(status,LV_LABEL_LONG_MODE_WRAP);
        lv_label_set_text(status,"Load a matching NGTR v3 language model for suggestions and autocorrect. Tries are embedded in the project.");
        struct Picker {PropertyWindow* window;lv_obj_t* status;};
        lv_obj_add_event_cb(button,[](lv_event_t* event) {
            auto* picker=static_cast<Picker*>(lv_event_get_user_data(event));
            if(lv_event_get_code(event)==LV_EVENT_DELETE){delete picker;return;}
            if(lv_event_get_code(event)!=LV_EVENT_CLICKED)return;
#ifdef _WIN32
            wchar_t path[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof dialog;
            dialog.hwndOwner=GetActiveWindow();dialog.lpstrFilter=L"NGTR trie (*.ngtr;*.trie;*.bin)\0*.ngtr;*.trie;*.bin\0All files\0*.*\0\0";
            dialog.lpstrFile=path;dialog.nMaxFile=_countof(path);dialog.lpstrTitle=L"Load prediction and autocorrect trie";
            dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
            if(!GetOpenFileNameW(&dialog))return;
            try {LoadTrie(picker->window,std::filesystem::path(path));}
            catch(const std::exception& error){lv_label_set_text(picker->status,error.what());}
#else
            const auto path=NativeFileDialogUI::PickOne(false,false,L"Load prediction and autocorrect trie",L"NGTR trie\0*.ngtr;*.trie;*.bin\0All files\0*.*\0\0");
            if(path.empty())return;
            try{LoadTrie(picker->window,path);}catch(const std::exception& error){lv_label_set_text(picker->status,error.what());}
#endif
        },LV_EVENT_ALL,new Picker{window,status});

        auto* targetField=Column(parent);
        auto* targetTitle=lv_label_create(targetField);lv_label_set_text(targetTitle,"Target text box");
        auto* target=lv_dropdown_create(targetField);lv_obj_set_width(target,LV_PCT(100));
        struct Target {PropertyWindow* window;lv_obj_t* object;std::vector<int> ids;};
        auto* binding=new Target{window,window->GetSelectedObject(),{-1}};
        std::string options="Automatic (focused text box)";uint32_t targetIndex=0;
        const auto objects=window->GetObjectTree()->GetObjects();
        for(size_t ordinal=0;ordinal<objects.size();++ordinal) {
            auto* object=objects[ordinal];
            const std::string type=Serialization::ObjectSerializer::TypeName(object);
            if(type!="textarea" && type!="textBox")continue;
            bool inside=false;for(auto* ancestor=object;ancestor;ancestor=lv_obj_get_parent(ancestor))if(ancestor==binding->object){inside=true;break;}
            if(inside)continue;
            const auto* metadata=static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));if(!metadata)continue;
            binding->ids.push_back(static_cast<int>(ordinal));options+='\n';options+=metadata->objectName;
            if(settings.value("targetOid",-1)==static_cast<int>(ordinal))targetIndex=static_cast<uint32_t>(binding->ids.size()-1);
        }
        lv_dropdown_set_options(target,options.c_str());lv_dropdown_set_selected(target,targetIndex);
        lv_obj_add_event_cb(target,[](lv_event_t* event) {
            auto* state=static_cast<Target*>(lv_event_get_user_data(event));
            if(lv_event_get_code(event)==LV_EVENT_DELETE){delete state;return;}
            if(lv_event_get_code(event)==LV_EVENT_CLICKED){FocusEditorControl(state->window->GetKBGroup(),lv_event_get_target_obj(event));return;}
            if(lv_event_get_code(event)!=LV_EVENT_VALUE_CHANGED || state->window->Drawing())return;
            const auto choice=lv_dropdown_get_selected(lv_event_get_target_obj(event));if(choice>=state->ids.size())return;
            const auto previous=Serialization::AdvancedKeyboard::ToJSON(state->object).at("advancedKeyboard");
            try {
                EditorEdit::Scope edit;
                state->window->Tries().ApplySettings(state->object,{{"targetOid",state->ids[choice]},{"autoFocus",true}});
                Serialization::AdvancedKeyboard::BindTargets(state->window->ProjectScreen(),state->window->GetObjectTree()->GetObjects());
            } catch(const std::exception&) {
                state->window->Tries().ApplySettings(state->object,previous);
                lv_obj_set_style_border_color(lv_event_get_target_obj(event),lv_color_hex(EditorStyle::Error),LV_PART_MAIN);
            }
        },LV_EVENT_ALL,binding);
    }

    static bool Supports(lv_obj_t* object)
    {
        const auto* type = lv_obj_get_class(object);
        return Serialization::AdvancedKeyboard::Type(object) || Serialization::CodeWidgets::Type(object) || Serialization::ExtendedWidgets::Type(object) || type == &lv_calendar_class || type == &lv_canvas_class || type == &lv_buttonmatrix_class;
    }

    static void Update(PropertyWindow* window, const json& record)
    {
        window->Drawing(true);
        auto* panel = window->ObjectPropWin();
        panel->DeleteChildren();
        panel->SetVisible(true);
        panel->UpdateHeight(LV_SIZE_CONTENT);
        auto* content = Column(panel->GetWindow());
        panel->AddObjectToWindow(content);
        if(record.contains("advancedKeyboard"))KeyboardControls(window,content,record.at("advancedKeyboard"));
        for (const auto& section : record.items())
        {
            if (section.key() == "base" || !section.value().is_object()) continue;
            for (const auto& field : section.value().items()) {
                if(section.key()=="advancedKeyboard" && (field.key()=="language" || field.key()=="trieId" || field.key()=="targetOid" || field.key()=="hideOnDefocus"))continue;
                Build(window, content, "/" + section.key() + "/" + field.key(), field.key(), field.value(), record);
            }
        }
        auto* status = lv_label_create(panel->GetWindow());
        lv_obj_set_width(status, LV_PCT(100));
        lv_label_set_text(status, "");
        // Keep status in the managed content so switching selection frees it.
        lv_obj_set_parent(status, content);
        window->CurrentlyLoadedProp = PropertyWindow::NONE;
        window->Drawing(false);
    }
};
#endif
