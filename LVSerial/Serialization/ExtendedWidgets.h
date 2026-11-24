#pragma once
#include "WidgetRecords.h"
#include "RuntimeFeatures.h"
#include "ObjectSerializer.h"
#include "ImageRecords.h"
#include "Style.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
// LVGL 9.6 has no public getters for chart axis ranges and tile directions.
#if __has_include(<src/widgets/chart/lv_chart_private.h>)
#include <src/widgets/chart/lv_chart_private.h>
#endif
#if __has_include(<src/widgets/tileview/lv_tileview_private.h>)
#include <src/widgets/tileview/lv_tileview_private.h>
#endif
namespace Serialization
{
    class ExtendedWidgets
    {
        struct GaugeData { lv_obj_t* needle = nullptr; };
        struct PanelData
        {
            std::map<lv_obj_t*, uint32_t> ids;
            uint32_t next = 1;
        };

        static uint32_t PanelID(lv_obj_t* object, lv_obj_t* panel)
        {
            auto& state = WidgetData<PanelData>(object);
            auto found = state.ids.find(panel);
            if (found != state.ids.end()) return found->second;
            return state.ids[panel] = state.next++;
        }

        static bool Managed(lv_obj_t* object)
        {
            const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
            return data && data->toolTray;
        }

        static bool ContainsManaged(lv_obj_t* object)
        {
            if (Managed(object)) return true;
            for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
                if (ContainsManaged(lv_obj_get_child(object, i))) return true;
            return false;
        }

        static void ReplaceChildren(lv_obj_t* parent, const json& children)
        {
            for (int32_t i = static_cast<int32_t>(lv_obj_get_child_count(parent)) - 1; i >= 0; --i)
            {
                auto* child = lv_obj_get_child(parent, i);
                if (!ContainsManaged(child)) lv_obj_delete(child);
            }
            RestoreChildren(parent, children);
        }

        static bool Changed(const json& previous, const json& data, const char* key)
        {
            return data.contains(key) && (!previous.contains(key) || previous[key] != data[key]);
        }

        static auto PointCoordinate(const json& data, const char* key) -> decltype(lv_point_t{}.x)
        {
            using Coordinate = decltype(lv_point_t{}.x);
            // LVGL 8 can use 16-bit points; newer versions use wider fields.
            // Keep ordinary coordinates on integer paths, including values
            // outside the target field's range. Only fractional JSON uses FP.
            const auto found = data.find(key);
            if (found == data.end()) return 0;
            if (found->is_number_unsigned())
                return static_cast<Coordinate>((std::min)(found->get<uint64_t>(),
                    static_cast<uint64_t>((std::numeric_limits<Coordinate>::max)())));
            if (found->is_number_integer())
                return static_cast<Coordinate>((std::clamp)(found->get<int64_t>(),
                    static_cast<int64_t>((std::numeric_limits<Coordinate>::lowest)()),
                    static_cast<int64_t>((std::numeric_limits<Coordinate>::max)())));
            const auto value = found->get<double>();
            if (std::isnan(value)) throw std::invalid_argument("Chart cursor coordinate must be a number");
            if (value <= (std::numeric_limits<Coordinate>::lowest)()) return (std::numeric_limits<Coordinate>::lowest)();
            if (value >= (std::numeric_limits<Coordinate>::max)()) return (std::numeric_limits<Coordinate>::max)();
            return static_cast<Coordinate>(value);
        }

#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW || LV_USE_TILEVIEW
        static lv_obj_t* PanelContainer(lv_obj_t* object, bool tabs) {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
            if(tabs) return lv_tabview_get_content(object);
#endif
            return object;
        }
        static lv_obj_t* TabButton(lv_obj_t* object, uint32_t index) {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
            return lv_tabview_get_tab_button(object,index);
#else
            throw std::invalid_argument("Tabview disabled in this runtime");
#endif
        }
        static lv_obj_t* AddPanel(lv_obj_t* object,const json& record,bool tabs) {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
            if(tabs) return lv_tabview_add_tab(object,record.value("name",std::string("Tab")).c_str());
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TILEVIEW
            if(!tabs) return lv_tileview_add_tile(object,record.value("column",0u),record.value("row",0u),static_cast<lv_dir_t>(record.value("directions",int(LV_DIR_ALL))));
#endif
            throw std::invalid_argument("Panel widget disabled in this runtime");
        }
        static void RenameTab(lv_obj_t* object,uint32_t index,const char* name) {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
            lv_tabview_set_tab_text(object,index,name);
#endif
        }
        static void ConfigureTile(lv_obj_t* object,const json& record) {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TILEVIEW
            reinterpret_cast<lv_tileview_tile_t*>(object)->dir=static_cast<lv_dir_t>(record.value("directions",int(LV_DIR_ALL)));
#endif
        }
        static void ApplyPanels(lv_obj_t* object, const json& records, bool tabs)
        {
            auto* container = PanelContainer(object, tabs);
            auto& state = WidgetData<PanelData>(object);
            std::map<uint32_t, lv_obj_t*> existing;
            for (uint32_t i = 0; i < lv_obj_get_child_count(container); ++i)
            {
                auto* panel = lv_obj_get_child(container, i);
                existing[PanelID(object, panel)] = panel;
            }

            uint32_t index = 0;
            for (const auto& record : records)
            {
                auto id = record.value("id", 0u);
                if (!id) id = state.next++;
                state.next = (std::max)(state.next, id + 1);
                auto found = existing.find(id);
                lv_obj_t* panel;
                lv_obj_t* button = nullptr;
                if (found != existing.end())
                {
                    panel = found->second;
                    if (tabs) button = TabButton(object, lv_obj_get_index(panel));
                    existing.erase(found);
                }
                else if (tabs)
                {
                    panel = AddPanel(object, record, true);
                    button = TabButton(object, lv_obj_get_index(panel));
                }
                else panel = AddPanel(object, record, false);
                state.ids[panel] = id;
                const auto old = ReadSection(panel, "panel");
                lv_obj_move_to_index(panel, index);
                if (tabs)
                {
                    lv_obj_move_to_index(button, index);
                    RenameTab(object, index, record.value("name", std::string("Tab")).c_str());
                    if (Changed(old, record, "buttonBase")) LVObject::FromJSON(record["buttonBase"], button);
                }
                else
                {
                    lv_obj_set_pos(panel, LV_PCT(record.value("column", 0) * 100), LV_PCT(record.value("row", 0) * 100));
                    ConfigureTile(panel, record);
                }
                if (Changed(old, record, "base")) LVObject::FromJSON(record["base"], panel);
                if (!lv_obj_get_user_data(panel)) LVObject::FromJSON(json::object(), panel);
                if (Changed(old, record, "children")) ReplaceChildren(panel, record["children"]);
                PreserveSection(panel, "panel", record);
                ++index;
            }
            for (const auto& entry : existing)
            {
                auto* panel = entry.second;
                if (tabs) lv_obj_delete(TabButton(object, lv_obj_get_index(panel)));
                state.ids.erase(panel);
                lv_obj_delete(panel);
            }
        }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGEBUTTON
        struct ImageSources
        {
            std::array<SerializedImageSource, LV_IMAGEBUTTON_STATE_NUM * 3> sources;
        };

        static json SaveImageSource(lv_obj_t* object,size_t index,const void* source)
        {
            auto& stored=WidgetData<ImageSources>(object).sources[index];
            if((source==&stored.descriptor||source==stored.providedSource)&&stored.assetRecord.is_object())return stored.assetRecord;
            return SaveImageSourceRecord(source);
        }

        static const void* RestoreImageSource(lv_obj_t* object, size_t index, const json& record)
        {
            return RestoreImageSourceRecord(record, WidgetData<ImageSources>(object).sources[index]);
        }
#endif
    public:
        static const char* Type(lv_obj_t* object)
        {
            const auto* type = lv_obj_get_class(object);
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHART
            if (type == &lv_chart_class) return "chart";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABLE
            if (type == &lv_table_class) return "table";
#endif
#if LVGL_VERSION_MAJOR == 8
            if (ui_compat_find(object,3)) return "scale";
            if (ui_compat_find(object,1)) return "tabview";
            if (ui_compat_find(object,2)) return "messagebox";
#else
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SCALE
            if (type == &lv_scale_class) return "scale";
#endif
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_KEYBOARD
            if (type == &lv_keyboard_class) return "keyboard";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
            if (type == &lv_tabview_class) return "tabview";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TILEVIEW
            if (type == &lv_tileview_class) return "tileview";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_WIN
            if (type == &lv_win_class) return "window";
#endif
            if (type == &lv_obj_class)
            {
                const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
                if (data && data->objectJson.contains("window")) return "window";
                if (data && data->objectJson.contains("page")) return "page";
            }
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_MSGBOX
            if (type == &lv_msgbox_class) return "messagebox";
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGEBUTTON
            if (type == &lv_imagebutton_class) return "imagebutton";
#endif
            return nullptr;
        }

        static lv_obj_t* Create(const std::string& type)
        {
            auto* parent = lv_screen_active();
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHART
            if (type == "chart") return lv_chart_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABLE
            if (type == "table") return lv_table_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SCALE
            if (type == "scale") return lv_scale_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_KEYBOARD
            if (type == "keyboard") return lv_keyboard_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
            if (type == "tabview") return lv_tabview_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TILEVIEW
            if (type == "tileview") return lv_tileview_create(parent);
#endif
            if (type == "page")
            {
                auto* page = lv_obj_create(parent);
                LVObject::FromJSON(json::object(), page);
                PreserveSection(page, "page", json::object());
                return page;
            }
#if !LVSERIAL_MINIMAL_RUNTIME || LVSERIAL_USE_WINDOW
            if (type == "window")
            {
                auto* window = lv_obj_create(parent);
                lv_obj_set_flex_flow(window, LV_FLEX_FLOW_COLUMN);
                lv_obj_set_style_pad_all(window, 0, LV_PART_MAIN);
                lv_obj_set_style_pad_row(window, 0, LV_PART_MAIN);
                lv_obj_set_scrollable(window, false);
                auto* header = lv_obj_create(window);
                lv_obj_set_size(header, LV_PCT(100), 40);
                lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
                lv_obj_set_style_pad_all(header, 8, LV_PART_MAIN);
                lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
                lv_obj_set_scrollable(header, false);
                auto* content = lv_obj_create(window);
                lv_obj_set_width(content, LV_PCT(100));
                lv_obj_set_flex_grow(content, 1);
                lv_obj_set_style_pad_all(content, 12, LV_PART_MAIN);
                lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
                LVObject::FromJSON(json::object(), window);
                PreserveSection(window, "window", {{"headerHeight", 40}});
                return window;
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_MSGBOX
            if (type == "messagebox") return lv_msgbox_create(parent);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGEBUTTON
            if (type == "imagebutton") return lv_imagebutton_create(parent);
#endif
            return nullptr;
        }

        static lv_obj_t* WindowHeader(lv_obj_t* window) { return lv_obj_get_child(window, 0); }
        static lv_obj_t* WindowContent(lv_obj_t* window) { return lv_obj_get_child(window, 1); }

        static lv_obj_t* ContentParent(lv_obj_t* object)
        {
            const auto* type = Type(object);
            if (!type) return object;
            const std::string name = type;
#if !LVSERIAL_MINIMAL_RUNTIME || LVSERIAL_USE_WINDOW
            if (name == "window") return WindowContent(object);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_MSGBOX
            if (name == "messagebox") return lv_msgbox_get_content(object);
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
            if (name == "tabview" && lv_tabview_get_tab_count(object))
                return lv_obj_get_child(lv_tabview_get_content(object), lv_tabview_get_tab_active(object));
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TILEVIEW
            if (name == "tileview" && lv_tileview_get_tile_active(object)) return lv_tileview_get_tile_active(object);
#endif
            return object;
        }

#if LVSERIAL_MINIMAL_RUNTIME
        static json Children(lv_obj_t* parent,lv_obj_t* ignored=nullptr) {
            auto result=json::array();
            for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i) {
                auto* child=lv_obj_get_child(parent,i);
                if(child==ignored || IsDocumentObject(child) || Managed(child))continue;
                auto record=ObjectSerializer::SerializeObject(child);
                record["contentChildren"]=Children(child);
                result.push_back(std::move(record));
            }
            return result;
        }
#else
        static json Children(lv_obj_t* parent, lv_obj_t* ignored = nullptr)
        {
            auto result = json::array();
            for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i)
            {
                auto* child = lv_obj_get_child(parent, i);
                if (child == ignored || IsDocumentObject(child) || Managed(child)) continue;
                auto record = ObjectSerializer::SerializeObject(child);
                const bool footerButton = lv_obj_get_class(child) == &lv_msgbox_footer_button_class;
                const bool headerButton = lv_obj_get_class(child) == &lv_msgbox_header_button_class;
                if (footerButton) record["contentClass"] = "messageboxFooterButton";
                if (headerButton)
                {
                    record["contentClass"] = "messageboxHeaderButton";
                    auto* icon = lv_obj_get_child(child, 0);
                    const void* source = icon && lv_obj_get_class(icon) == &lv_image_class ? lv_image_get_src(icon) : nullptr;
                    record["close"] = source && lv_image_src_get_type(source) == LV_IMAGE_SRC_SYMBOL &&
                        std::string(static_cast<const char*>(source)) == LV_SYMBOL_CLOSE;
                }
                if (headerButton || footerButton || lv_obj_has_class(child, &lv_button_class) || lv_obj_get_class(child) == &lv_obj_class)
                    record["contentChildren"] = Children(child);
                result.push_back(std::move(record));
            }
            return result;
        }

#endif
        static void RestoreChildren(lv_obj_t* parent, const json& children)
        {
            if (!children.is_array()) return;
            for (const auto& child : children)
            {
                lv_obj_t* object;
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_MSGBOX
                if (child.value("contentClass", std::string{}) == "messageboxFooterButton")
                {
                    object = lv_msgbox_add_footer_button(lv_obj_get_parent(parent), nullptr);
                    LVObject::FromJSON(child.at("base"), object);
                }
                else if (child.value("contentClass", std::string{}) == "messageboxHeaderButton")
                {
                    auto* message = lv_obj_get_parent(parent);
                    object = child.value("close", false) ? lv_msgbox_add_close_button(message) : lv_msgbox_add_header_button(message, nullptr);
                    lv_obj_clean(object);
                    LVObject::FromJSON(child.at("base"), object);
                }
                else
#endif
                object = ObjectSerializer::DeserializeObject(child);
                if (object)
                {
                    lv_obj_set_parent(object, parent);
                    if (child.contains("contentChildren")) RestoreChildren(object, child["contentChildren"]);
                }
            }
        }

        static json ToJSON(lv_obj_t* object)
        {
#if defined(LV_RUNTIME_PORTABLE) || LVSERIAL_MINIMAL_RUNTIME
            auto* saved = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
            return saved ? saved->objectJson : json::object();
#else
            lv_obj_update_layout(object);
            const std::string type = Type(object);
            auto data = ReadSection(object, type.c_str());
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SCALE
            if (type == "scale")
            {
                data["mode"] = lv_scale_get_mode(object);
                data["min"] = lv_scale_get_range_min_value(object);
                data["max"] = lv_scale_get_range_max_value(object);
                data["ticks"] = lv_scale_get_total_tick_count(object);
                data["majorEvery"] = lv_scale_get_major_tick_every(object);
                data["rotation"] = lv_scale_get_rotation(object);
                data["angleRange"] = lv_scale_get_angle_range(object);
                data["showLabels"] = lv_scale_get_label_show(object);
                auto* needle = WidgetData<GaugeData>(object).needle;
                if (data.value("variant", std::string{}) == "gauge")
                {
                    data["value"] = data.value("value", 50);
                    data["needleLength"] = data.value("needleLength", 50);
                    data["needleColor"] = needle ? lv_color_to_u32(lv_obj_get_style_line_color(needle, LV_PART_MAIN)) : 0xffeeeeee;
                    data["needleWidth"] = needle ? lv_obj_get_style_line_width(needle, LV_PART_MAIN) : 3;
                }
                data["children"] = Children(object, needle);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_KEYBOARD
            if (type == "keyboard")
            {
                data["mode"] = lv_keyboard_get_mode(object);
                data["popovers"] = lv_keyboard_get_popovers(object);
                data["buttons"] = LVButtonMatrix::ToJSON(object)["btnm"];
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHART
            if (type == "chart")
            {
                const auto* chart = reinterpret_cast<const lv_chart_t*>(object);
                data["axes"] = json::array();
                for (int i = 0; i < 2; ++i)
                {
                    data["axes"].push_back({{"axis", i}, {"min", chart->ymin[i]}, {"max", chart->ymax[i]}});
                    data["axes"].push_back({{"axis", i == 0 ? LV_CHART_AXIS_PRIMARY_X : LV_CHART_AXIS_SECONDARY_X}, {"min", chart->xmin[i]}, {"max", chart->xmax[i]}});
                }
                data["type"] = lv_chart_get_type(object);
                data["pointCount"] = lv_chart_get_point_count(object);
                data["updateMode"] = lv_chart_get_update_mode(object);
                data["horizontalDivisions"] = lv_chart_get_hor_div_line_count(object);
                data["verticalDivisions"] = lv_chart_get_ver_div_line_count(object);
                data["series"] = json::array();
                lv_chart_series_t* series = nullptr;
                while ((series = lv_chart_get_series_next(object, series)))
                {
                    json record{{"color", lv_color_to_u32(lv_chart_get_series_color(object, series))},
                        {"start", lv_chart_get_x_start_point(object, series)}, {"y", json::array()}};
                    record["axis"] = (series->x_axis_sec ? LV_CHART_AXIS_SECONDARY_X : 0) | (series->y_axis_sec ? LV_CHART_AXIS_SECONDARY_Y : 0);
                    record["hidden"] = bool(series->hidden);
                    const auto* y = lv_chart_get_series_y_array(object, series);
                    const auto* x = lv_chart_get_series_x_array(object, series);
                    if (x) record["x"] = json::array();
                    for (uint32_t i = 0; i < lv_chart_get_point_count(object); ++i)
                    {
                        record["y"].push_back(y[i]);
                        if (x) record["x"].push_back(x[i]);
                    }
                    data["series"].push_back(std::move(record));
                }
                data["cursors"] = json::array();
                for (auto* cursor = static_cast<lv_chart_cursor_t*>(lv_ll_get_head(&chart->cursor_ll)); cursor;
                     cursor = static_cast<lv_chart_cursor_t*>(lv_ll_get_next(&chart->cursor_ll, cursor)))
                {
                    int index = -1, current = 0;
                    lv_chart_series_t* candidate = nullptr;
                    while ((candidate = lv_chart_get_series_next(object, candidate)))
                    { if (!cursor->pos_set && candidate == cursor->ser) index = current; ++current; }
                    data["cursors"].push_back({{"color", lv_color_to_u32(cursor->color)}, {"directions", cursor->dir},
                        {"positionSet", bool(cursor->pos_set)}, {"x", cursor->pos.x}, {"y", cursor->pos.y},
                        {"series", index}, {"point", cursor->point_id}});
                }
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABLE
            if (type == "table")
            {
                data["columns"] = json::array(); data["rows"] = json::array();
                for (uint32_t column = 0; column < lv_table_get_column_count(object); ++column)
                    data["columns"].push_back(lv_table_get_column_width(object, column));
                for (uint32_t row = 0; row < lv_table_get_row_count(object); ++row)
                {
                    auto cells = json::array();
                    for (uint32_t column = 0; column < lv_table_get_column_count(object); ++column)
                    {
                        uint32_t flags = 0;
                        for (uint32_t flag = 1; flag <= LV_TABLE_CELL_CTRL_CUSTOM_4; flag <<= 1)
                            if (lv_table_has_cell_ctrl(object, row, column, static_cast<lv_table_cell_ctrl_t>(flag))) flags |= flag;
                        cells.push_back({{"text", lv_table_get_cell_value(object, row, column)}, {"flags", flags}});
                    }
                    data["rows"].push_back(std::move(cells));
                }
                uint32_t row, column;
                lv_table_get_selected_cell(object, &row, &column);
                data["selectedRow"] = row; data["selectedColumn"] = column;
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
            if (type == "tabview")
            {
                data["active"] = lv_tabview_get_tab_active(object);
                data["position"] = lv_tabview_get_tab_bar_position(object);
                auto* bar = lv_tabview_get_tab_bar(object);
                data["barBase"] = LVObject::ToJSON(bar);
                data["contentBase"] = LVObject::ToJSON(lv_tabview_get_content(object));
                data["barSize"] = data["position"] == LV_DIR_TOP || data["position"] == LV_DIR_BOTTOM ? lv_obj_get_height(bar) : lv_obj_get_width(bar);
                data["tabs"] = json::array();
                auto* content = lv_tabview_get_content(object);
                for (uint32_t i = 0; i < lv_tabview_get_tab_count(object); ++i)
                {
                    auto* button = lv_tabview_get_tab_button(object, i);
                    const auto* label = lv_obj_get_child(button, 0);
                    auto* panel = lv_obj_get_child(content, i);
                    data["tabs"].push_back({{"id", PanelID(object, panel)}, {"name", lv_label_get_text(label)}, {"buttonBase", LVObject::ToJSON(button)},
                        {"base", LVObject::ToJSON(panel)}, {"children", Children(panel)}});
                }
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LVSERIAL_USE_WINDOW
            if (type == "window")
            {
                data["headerHeight"] = lv_obj_get_height(WindowHeader(object));
                data["headerBase"] = LVObject::ToJSON(WindowHeader(object));
                data["contentBase"] = LVObject::ToJSON(WindowContent(object));
                data["header"] = Children(WindowHeader(object));
                data["children"] = Children(WindowContent(object));
                for (uint32_t i = 0; i < lv_obj_get_child_count(WindowHeader(object)); ++i)
                {
                    auto* child = lv_obj_get_child(WindowHeader(object), i);
                    if (lv_obj_get_class(child) == &lv_label_class) { data["title"] = lv_label_get_text(child); break; }
                }
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_MSGBOX
            if (type == "messagebox")
            {
                auto* title = lv_msgbox_get_title(object);
                data["title"] = title ? lv_label_get_text(title) : "";
                data["children"] = Children(lv_msgbox_get_content(object));
                auto* header = lv_msgbox_get_header(object);
                data["headerButtons"] = header ? Children(header, title) : json::array();
                if (header) data["headerBase"] = LVObject::ToJSON(header);
                data["contentBase"] = LVObject::ToJSON(lv_msgbox_get_content(object));
                auto* footer = lv_msgbox_get_footer(object);
                data["footer"] = footer ? Children(footer) : json::array();
                if (footer) data["footerBase"] = LVObject::ToJSON(footer);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TILEVIEW
            if (type == "tileview")
            {
                data["tiles"] = json::array();
                for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
                {
                    auto* tile = lv_obj_get_child(object, i);
                    if (lv_obj_get_class(tile) != &lv_tileview_tile_class) continue;
                    const auto* native = reinterpret_cast<const lv_tileview_tile_t*>(tile);
                    data["tiles"].push_back({{"id", PanelID(object, tile)}, {"column", lv_obj_get_x(tile) / (std::max)(1, lv_obj_get_content_width(object))},
                        {"row", lv_obj_get_y(tile) / (std::max)(1, lv_obj_get_content_height(object))},
                        {"directions", native->dir}, {"base", LVObject::ToJSON(tile)}, {"children", Children(tile)}});
                    if (tile == lv_tileview_get_tile_active(object)) data["active"] = data["tiles"].size() - 1;
                }
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGEBUTTON
            if (type == "imagebutton")
            {
                data["states"] = json::array();
                for (int i = 0; i < LV_IMAGEBUTTON_STATE_NUM; ++i)
                {
                    const auto state = static_cast<lv_imagebutton_state_t>(i);
                    data["states"].push_back({SaveImageSource(object,i*3,lv_imagebutton_get_src_left(object, state)),
                        SaveImageSource(object,i*3+1,lv_imagebutton_get_src_middle(object, state)), SaveImageSource(object,i*3+2,lv_imagebutton_get_src_right(object, state))});
                }
                data["children"] = Children(object);
            }
#endif
            if (type == "page")
            {
                data["scrollDirection"] = lv_obj_get_scroll_dir(object);
                data["scrollbarMode"] = lv_obj_get_scrollbar_mode(object);
                data["children"] = Children(object);
            }
            auto& styles = data["styles"];
            for (const auto& part : {std::make_pair("Main", LV_PART_MAIN), std::make_pair("Items", LV_PART_ITEMS),
                std::make_pair("Indicator", LV_PART_INDICATOR), std::make_pair("Cursor", LV_PART_CURSOR)})
                styles[part.first] = Style::Read(object, part.second, styles[part.first]);
            return {{"base", LVObject::ToJSON(object)}, {type, data}};
        #endif
        }

        static void Apply(lv_obj_t* object, const json& data, bool applyStyles = true)
        {
            const std::string name = Type(object);
            const auto previous = ReadSection(object, name.c_str());
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_SCALE
            if (name == "scale")
            {
                if (data.value("min", 0) >= data.value("max", 100)) throw std::invalid_argument("Maximum must exceed minimum");
                lv_scale_set_mode(object, static_cast<lv_scale_mode_t>(data.value("mode", 0)));
                lv_scale_set_range(object, data.value("min", 0), data.value("max", 100));
                lv_scale_set_total_tick_count(object, (std::max)(2, data.value("ticks", 11)));
                lv_scale_set_major_tick_every(object, (std::max)(1, data.value("majorEvery", 5)));
                lv_scale_set_rotation(object, data.value("rotation", 0));
                lv_scale_set_angle_range(object, data.value("angleRange", 270));
                lv_scale_set_label_show(object, data.value("showLabels", true));
                auto& gauge = WidgetData<GaugeData>(object);
                if (data.value("variant", std::string{}) == "gauge")
                {
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LINE
                    if (!gauge.needle) gauge.needle = lv_line_create(object);
                    lv_obj_set_style_line_color(gauge.needle, lv_color_hex(data.value("needleColor", 0xffeeeeeeu)), LV_PART_MAIN);
                    lv_obj_set_style_line_width(gauge.needle, (std::max)(1, data.value("needleWidth", 3)), LV_PART_MAIN);
                    lv_obj_update_layout(object);
                    lv_scale_set_line_needle_value(object, gauge.needle, data.value("needleLength", 50),
                        std::clamp(data.value("value", 50), lv_scale_get_range_min_value(object), lv_scale_get_range_max_value(object)));
#else
                    throw std::invalid_argument("Gauge needle requires LV_USE_LINE");
#endif
                }
                if (Changed(previous, data, "children"))
                {
                    // The gauge needle is maintained by the scale value setter.
                    auto* needle = gauge.needle;
                    if (needle) lv_obj_set_parent(needle, lv_layer_top());
                    ReplaceChildren(object, data["children"]);
                    if (needle) lv_obj_set_parent(needle, object);
                }
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_KEYBOARD
            if (name == "keyboard")
            {
                lv_keyboard_set_mode(object, static_cast<lv_keyboard_mode_t>(data.value("mode", 0)));
                lv_keyboard_set_popovers(object, data.value("popovers", false));
                if (data.contains("buttons") && data["buttons"].is_object() &&
                    (!Changed(previous, data, "mode") || Changed(previous, data, "buttons")))
                {
                    LVButtonMatrix::Apply(object, data["buttons"], applyStyles);
                }
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHART
            if (name == "chart")
            {
                const auto count = data.value("pointCount", 10u);
                if (count < 1 || count > 65535) throw std::invalid_argument("Point count must be between 1 and 65535");
                auto* chart = reinterpret_cast<lv_chart_t*>(object);
                while (auto* cursor = static_cast<lv_chart_cursor_t*>(lv_ll_get_head(&chart->cursor_ll)))
                    lv_chart_remove_cursor(object, cursor);
                while (auto* series = lv_chart_get_series_next(object, nullptr)) lv_chart_remove_series(object, series);
                lv_chart_set_type(object, static_cast<lv_chart_type_t>(data.value("type", 1)));
                lv_chart_set_point_count(object, (std::max)(1u, data.value("pointCount", 10u)));
                lv_chart_set_update_mode(object, static_cast<lv_chart_update_mode_t>(data.value("updateMode", 0)));
                lv_chart_set_div_line_count(object, data.value("horizontalDivisions", 3u), data.value("verticalDivisions", 5u));
                if (data.contains("axes")) for (const auto& axis : data["axes"])
                    lv_chart_set_axis_range(object, static_cast<lv_chart_axis_t>(axis.value("axis", 0)), axis.value("min", 0), axis.value("max", 100));
                std::vector<lv_chart_series_t*> restoredSeries;
                if (data.contains("series")) for (const auto& series : data["series"])
                {
                    auto* restored = lv_chart_add_series(object, lv_color_hex(series.value("color", 0xffffffu)), static_cast<lv_chart_axis_t>(series.value("axis", 0)));
                    restoredSeries.push_back(restored);
                    lv_chart_hide_series(object, restored, series.value("hidden", false));
                    for (uint32_t i = 0; i < lv_chart_get_point_count(object) && i < series["y"].size(); ++i)
                    {
                        lv_chart_get_series_y_array(object, restored)[i] = series["y"][i].get<int32_t>();
                        if (series.contains("x") && i < series["x"].size() && lv_chart_get_series_x_array(object, restored))
                            lv_chart_get_series_x_array(object, restored)[i] = series["x"][i].get<int32_t>();
                    }
                    lv_chart_set_x_start_point(object, restored, series.value("start", 0u));
                }
                if (data.contains("cursors")) for (auto iterator = data["cursors"].rbegin(); iterator != data["cursors"].rend(); ++iterator)
                {
                    const auto& item = *iterator;
                    auto* cursor = lv_chart_add_cursor(object, lv_color_hex(item.value("color", 0xffffffu)),
                        static_cast<lv_dir_t>(item.value("directions", int(LV_DIR_ALL))));
                    const int index = item.value("series", -1);
                    if (!item.value("positionSet", true) && index >= 0 && index < static_cast<int>(restoredSeries.size()))
                        lv_chart_set_cursor_point(object, cursor, restoredSeries[index],
                            (std::min<uint32_t>)(item.value("point", 0u), lv_chart_get_point_count(object) - 1));
                    else
                    {
                        lv_point_t position{PointCoordinate(item, "x"), PointCoordinate(item, "y")};
                        lv_chart_set_cursor_pos(object, cursor, &position);
                    }
                }
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABLE
            if (name == "table")
            {
                if (data.contains("columns"))
                {
                    lv_table_set_column_count(object, static_cast<uint32_t>(data["columns"].size()));
                    for (uint32_t i = 0; i < data["columns"].size(); ++i) lv_table_set_column_width(object, i, data["columns"][i].get<int32_t>());
                }
                if (data.contains("rows"))
                {
                    lv_table_set_row_count(object, static_cast<uint32_t>(data["rows"].size()));
                    for (uint32_t row = 0; row < data["rows"].size(); ++row)
                        for (uint32_t column = 0; column < data["rows"][row].size(); ++column)
                        {
                            const auto& cell = data["rows"][row][column];
                            lv_table_set_cell_value(object, row, column, cell.value("text", std::string{}).c_str());
                            lv_table_clear_cell_ctrl(object, row, column, static_cast<lv_table_cell_ctrl_t>(0xff));
                            lv_table_set_cell_ctrl(object, row, column, static_cast<lv_table_cell_ctrl_t>(cell.value("flags", 0u)));
                        }
                }
                lv_table_set_selected_cell(object, data.value("selectedRow", uint32_t(LV_TABLE_CELL_NONE)), data.value("selectedColumn", uint32_t(LV_TABLE_CELL_NONE)));
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TABVIEW
            if (name == "tabview")
            {
                lv_tabview_set_tab_bar_position(object, static_cast<lv_dir_t>(data.value("position", int(LV_DIR_TOP))));
                lv_tabview_set_tab_bar_size(object, data.value("barSize", 40));
                if (Changed(previous, data, "barBase")) LVObject::FromJSON(data["barBase"], lv_tabview_get_tab_bar(object));
                if (Changed(previous, data, "contentBase")) LVObject::FromJSON(data["contentBase"], lv_tabview_get_content(object));
                if (Changed(previous, data, "tabs")) ApplyPanels(object, data["tabs"], true);
                if (lv_tabview_get_tab_count(object))
                    lv_tabview_set_active(object, (std::min)(data.value("active", 0u), lv_tabview_get_tab_count(object) - 1), LV_ANIM_OFF);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LVSERIAL_USE_WINDOW
            if (name == "window")
            {
                lv_obj_set_height(WindowHeader(object), data.value("headerHeight", 40));
                if (Changed(previous, data, "headerBase")) LVObject::FromJSON(data["headerBase"], WindowHeader(object));
                if (Changed(previous, data, "contentBase")) LVObject::FromJSON(data["contentBase"], WindowContent(object));
                if (Changed(previous, data, "header")) ReplaceChildren(WindowHeader(object), data["header"]);
                if (Changed(previous, data, "children")) ReplaceChildren(WindowContent(object), data["children"]);
                if (data.contains("title"))
                {
                    auto* header = WindowHeader(object);
                    lv_obj_t* label = nullptr;
                    for (uint32_t i = 0; i < lv_obj_get_child_count(header); ++i)
                        if (lv_obj_get_class(lv_obj_get_child(header, i)) == &lv_label_class) { label = lv_obj_get_child(header, i); break; }
                    if (!label) label = lv_label_create(header);
                    lv_label_set_text(label, data["title"].get<std::string>().c_str());
                }
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_MSGBOX
            if (name == "messagebox")
            {
                auto* title = lv_msgbox_get_title(object);
                const auto text = data.value("title", std::string{});
                if (title) lv_label_set_text(title, text.c_str());
                else if (!text.empty() || data.contains("headerBase") || (data.contains("headerButtons") && !data["headerButtons"].empty()))
                    lv_msgbox_add_title(object, text.c_str());
                if (Changed(previous, data, "headerBase")) LVObject::FromJSON(data["headerBase"], lv_msgbox_get_header(object));
                if (Changed(previous, data, "contentBase")) LVObject::FromJSON(data["contentBase"], lv_msgbox_get_content(object));
                if (Changed(previous, data, "headerButtons"))
                {
                    auto* header = lv_msgbox_get_header(object);
                    if (header)
                    {
                        for (int32_t i = static_cast<int32_t>(lv_obj_get_child_count(header)) - 1; i >= 0; --i)
                        { auto* child = lv_obj_get_child(header, i); if (child != lv_msgbox_get_title(object) && !ContainsManaged(child)) lv_obj_delete(child); }
                        RestoreChildren(header, data["headerButtons"]);
                    }
                }
                if (Changed(previous, data, "children")) ReplaceChildren(lv_msgbox_get_content(object), data["children"]);
                if (Changed(previous, data, "footer") && (!data["footer"].empty() || lv_msgbox_get_footer(object) || data.contains("footerBase")))
                {
                    if (!lv_msgbox_get_footer(object))
                        lv_obj_delete(lv_msgbox_add_footer_button(object, nullptr));
                    ReplaceChildren(lv_msgbox_get_footer(object), data["footer"]);
                }
                if (Changed(previous, data, "footerBase") && lv_msgbox_get_footer(object)) LVObject::FromJSON(data["footerBase"], lv_msgbox_get_footer(object));
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_TILEVIEW
            if (name == "tileview")
            {
                if (Changed(previous, data, "tiles")) ApplyPanels(object, data["tiles"], false);
                const auto active = data.value("active", 0u);
                if (active < lv_obj_get_child_count(object))
                    lv_tileview_set_tile(object, lv_obj_get_child(object, active), LV_ANIM_OFF);
            }
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGEBUTTON
            if (name == "imagebutton")
            {
                if (Changed(previous, data, "states")) for (size_t state = 0; state < data["states"].size() && state < LV_IMAGEBUTTON_STATE_NUM; ++state)
                {
                    const auto& sources = data["states"][state];
                    if (!sources.is_array() || sources.size() != 3) continue;
                    lv_imagebutton_set_src(object, static_cast<lv_imagebutton_state_t>(state),
                        RestoreImageSource(object, state * 3, sources[0]), RestoreImageSource(object, state * 3 + 1, sources[1]),
                        RestoreImageSource(object, state * 3 + 2, sources[2]));
                }
                if (Changed(previous, data, "children")) ReplaceChildren(object, data["children"]);
            }
#endif
            if (name == "page")
            {
                lv_obj_set_scrollable(object, true);
                lv_obj_set_scroll_dir(object, static_cast<lv_dir_t>(data.value("scrollDirection", int(LV_DIR_VER))));
                lv_obj_set_scrollbar_mode(object, static_cast<lv_scrollbar_mode_t>(data.value("scrollbarMode", int(LV_SCROLLBAR_MODE_AUTO))));
                if (Changed(previous, data, "children")) ReplaceChildren(object, data["children"]);
            }
            if (applyStyles && data.contains("styles"))
                for (const auto& part : {std::make_pair("Main", LV_PART_MAIN), std::make_pair("Items", LV_PART_ITEMS),
                    std::make_pair("Indicator", LV_PART_INDICATOR), std::make_pair("Cursor", LV_PART_CURSOR)})
                    if (data["styles"].contains(part.first) && (!previous.contains("styles") ||
                        !previous["styles"].contains(part.first) || previous["styles"][part.first] != data["styles"][part.first]))
                        Style::Apply(object, data["styles"][part.first], part.second);
            PreserveSection(object, name.c_str(), data);
        }

        static lv_obj_t* FromJSON(const json& record)
        {
            for (const auto* type : {"chart", "table", "scale", "keyboard", "tabview", "tileview", "window", "messagebox", "imagebutton", "page"})
            {
                if (!record.contains(type)) continue;
                auto* object = Create(type);
                if (!object) throw std::invalid_argument(std::string("Widget disabled in this runtime: ")+type);
                LVObject::FromJSON(record.at("base"), object);
                Apply(object, record.at(type), !HasNativeStyleSnapshot(record.at("base")));
                LVObject::FromJSON(record.at("base"), object);
                return object;
            }
            return nullptr;
        }
    };
}
#endif
