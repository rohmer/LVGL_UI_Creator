#pragma once

#include "Canvas.h"
#include <Serialization/ExtendedWidgets.h>
#include <Serialization/LVCanvas.h>
#include <Serialization/CodeWidgets.h>
#include <Serialization/AdvancedKeyboard.h>

namespace AdvancedWidget
{
#if LVGL_VERSION_MAJOR >= 9
    inline lv_obj_t* Create(int id, lv_obj_t* parent, int x, int y, std::string& name)
    {
        using namespace Serialization;
        lv_obj_t* object = nullptr;
        json settings;
        std::string type;
        int width = 240;
        int height = 160;
        switch (id)
        {
        case 34:
            name = "Advanced Keyboard";
            object = AdvancedKeyboard::Create(parent);
            if (!object) return nullptr;
            try { AdvancedKeyboard::Apply(object, json::object()); }
            catch (...) { lv_obj_delete(object); throw; }
            lv_obj_set_size(object, 300, 144);
            lv_obj_align(object,LV_ALIGN_BOTTOM_MID,0,0);
            return object;
        case 32:
        case 33:
            name = id == 32 ? "Barcode" : "QR code";
            object = CodeWidgets::Create(id == 32 ? "barcode" : "qrcode", parent);
            if (!object) return nullptr;
            try { CodeWidgets::Apply(object, json::object()); }
            catch (...) { lv_obj_delete(object); throw; }
            lv_obj_set_pos(object, x, y);
            return object;
        case 5:
            name = "Canvas";
            object = Canvas::Create(parent, x, y);
            LVObject::FromJSON(json::object(), object);
            LVCanvas::Apply(object, {{"width", 144}, {"height", 96}, {"format", LV_COLOR_FORMAT_ARGB8888},
                {"fillColor", 0xff447788u}, {"fillOpacity", 255}});
            lv_obj_set_size(object, 144, 96);
            return object;
        case 7:
            name = "Chart";
            type = "chart";
            settings = {{"type", LV_CHART_TYPE_LINE}, {"pointCount", 5}, {"updateMode", LV_CHART_UPDATE_MODE_SHIFT},
                {"horizontalDivisions", 3}, {"verticalDivisions", 4},
                {"series", {{{"color", 0xff4caf50u}, {"axis", LV_CHART_AXIS_PRIMARY_Y}, {"y", {20, 45, 30, 75, 60}}}}}};
            break;
        case 10:
        case 17:
            name = id == 10 ? "Gauge" : "Line meter";
            type = "scale";
            width = height = 150;
            settings = {{"variant", id == 10 ? "gauge" : "lineMeter"}, {"mode", LV_SCALE_MODE_ROUND_INNER},
                {"min", 0}, {"max", 100}, {"ticks", 21}, {"majorEvery", 5}, {"rotation", 135},
                {"angleRange", 270}, {"showLabels", true}};
            if (id == 10)
            {
                settings["value"] = 50;
                settings["needleLength"] = 50;
                settings["needleColor"] = 0xffeeeeeeu;
                settings["needleWidth"] = 3;
            }
            break;
        case 12:
            name = "Image button";
            type = "imagebutton";
            width = 120;
            height = 44;
            break;
        case 13:
            name = "Keyboard";
            type = "keyboard";
            width = 280;
            height = 130;
            settings = {{"mode", LV_KEYBOARD_MODE_TEXT_LOWER}, {"popovers", false}};
            break;
        case 18:
            name = "Message box";
            type = "messagebox";
            width = 260;
            height = 180;
            settings = {{"title", "Message"}};
            break;
        case 19:
            name = "Page";
            type = "page";
            width = 220;
            settings = {{"scrollDirection", LV_DIR_VER}, {"scrollbarMode", LV_SCROLLBAR_MODE_AUTO}};
            break;
        case 25:
            name = "Table";
            type = "table";
            width = 220;
            height = 120;
            settings = {{"columns", {100, 100}}, {"rows", {
                {{{"text", "Name"}, {"flags", 0}}, {{"text", "Value"}, {"flags", 0}}},
                {{{"text", "Item"}, {"flags", 0}}, {{"text", "100"}, {"flags", 0}}}}}};
            break;
        case 26:
            name = "Tabview";
            type = "tabview";
            width = 260;
            height = 180;
            settings = {{"active", 0}, {"position", LV_DIR_TOP}, {"barSize", 36},
                {"tabs", {{{"name", "Tab 1"}}, {{"name", "Tab 2"}}}}};
            break;
        case 28:
            name = "Tile view";
            type = "tileview";
            width = 260;
            height = 180;
            settings = {{"active", 0}, {"tiles", {
                {{"column", 0}, {"row", 0}, {"directions", LV_DIR_RIGHT}},
                {{"column", 1}, {"row", 0}, {"directions", LV_DIR_LEFT}}}}};
            break;
        case 29:
            name = "Window";
            type = "window";
            width = 280;
            height = 200;
            settings = {{"title", "Window"}, {"headerHeight", 36}};
            break;
        default:
            return nullptr;
        }
        object = ExtendedWidgets::Create(type);
        if (!lv_obj_get_user_data(object)) LVObject::FromJSON(json::object(), object);
        lv_obj_set_parent(object, parent);
        lv_obj_set_size(object, width, height);
        lv_obj_set_pos(object, x, y);
        ExtendedWidgets::Apply(object, settings.is_object() ? settings : json::object());
        if (id == 12)
        {
            auto source = json{{"kind", "pixels"}, {"formatVersion", 9}, {"cf", LV_COLOR_FORMAT_ARGB8888},
                {"w", 2}, {"h", 36}, {"stride", 8}};
            auto states = json::array();
            for (int state = 0; state < LV_IMAGEBUTTON_STATE_NUM; ++state)
            {
                std::vector<uint8_t> pixels(2 * 36 * 4);
                for (size_t pixel = 0; pixel < pixels.size(); pixel += 4)
                {
                    pixels[pixel] = 0x60;
                    pixels[pixel + 1] = state == LV_IMAGEBUTTON_STATE_PRESSED ? 0x70 : 0xa0;
                    pixels[pixel + 2] = 0x40;
                    pixels[pixel + 3] = 255;
                }
                source["data"] = base64_encode(pixels.data(), static_cast<unsigned int>(pixels.size()));
                states.push_back({nullptr, source, nullptr});
            }
            ExtendedWidgets::Apply(object, {{"states", states}});
            auto* label = lv_label_create(object);
            lv_label_set_text(label, "Image button");
            lv_obj_center(label);
        }
        else if (id == 18)
        {
            lv_msgbox_add_text(object, "Message text");
            lv_msgbox_add_footer_button(object, "OK");
            lv_msgbox_add_footer_button(object, "Cancel");
        }
        else if (id == 29)
        {
            auto* label = lv_label_create(ExtendedWidgets::WindowContent(object));
            lv_label_set_text(label, "Window content");
        }
        lv_obj_set_size(object, width, height);
        return object;
    }
#endif
}
