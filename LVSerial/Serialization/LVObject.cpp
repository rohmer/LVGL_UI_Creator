#include "LVObject.h"
#include "NativeStyles.h"
#include "BootstrapActions.h"

#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include <algorithm>
#include "Style.h"

namespace Serialization
{
    namespace
    {
        void FreeObjectUserData(lv_event_t* event)
        {
            delete static_cast<ObjectUserData*>(lv_event_get_user_data(event));
        }

        const json& ReadObject(const json& parent, const char* key)
        {
            static const json empty = json::object();
            const auto entry = parent.find(key);
            return entry != parent.end() && entry->is_object() ? *entry : empty;
        }

        void ApplyBaseStyle(lv_obj_t* object, const json& style)
        {
            const auto& body = ReadObject(style, "body");
            const auto& border = ReadObject(body, "border");
            const auto& padding = ReadObject(body, "padding");
            const auto& text = ReadObject(style, "text");
            const auto& shadow = ReadObject(body, "shadow");

            auto number = [object](const json& properties, const char* key, auto setter) {
                const auto value = properties.find(key);
                if (value != properties.end() && value->is_number_integer())
                    setter(object, value->get<int>(), LV_PART_MAIN);
            };
            auto color = [object](const json& properties, const char* key, auto setter) {
                const auto value = properties.find(key);
                if (value != properties.end() && value->is_number_integer())
                    setter(object, lv_color_hex(value->get<uint32_t>()), LV_PART_MAIN);
            };

            color(body, "main_color", lv_obj_set_style_bg_color);
            color(body, "gradColor", lv_obj_set_style_bg_grad_color);
            if (body.contains("gradColor"))
                lv_obj_set_style_bg_grad_dir(object, LV_GRAD_DIR_VER, LV_PART_MAIN);
            number(body, "opa", lv_obj_set_style_bg_opa);
            number(body, "radius", lv_obj_set_style_radius);
            color(border, "color", lv_obj_set_style_border_color);
            number(border, "opa", lv_obj_set_style_border_opa);
            number(border, "width", lv_obj_set_style_border_width);
            number(padding, "left", lv_obj_set_style_pad_left);
            number(padding, "right", lv_obj_set_style_pad_right);
            number(padding, "top", lv_obj_set_style_pad_top);
            number(padding, "bottom", lv_obj_set_style_pad_bottom);
            number(padding, "inner", lv_obj_set_style_pad_row);
            number(padding, "inner", lv_obj_set_style_pad_column);
            color(shadow, "color", lv_obj_set_style_shadow_color);
            number(shadow, "width", lv_obj_set_style_shadow_width);
            color(text, "color", lv_obj_set_style_text_color);
            number(text, "letter_space", lv_obj_set_style_text_letter_space);
            number(text, "line_space", lv_obj_set_style_text_line_space);
            number(text, "opa", lv_obj_set_style_text_opa);
        }
    }

    json LVObject::ToJSON(lv_obj_t* object)
    {
        const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
        json result = data ? ReadObject(data->objectJson, "base") : json::object();
        result["name"] = data ? data->objectName : "";
        lv_obj_update_layout(object);
        lv_point_t position{lv_obj_get_x(object), lv_obj_get_y(object)};
        lv_point_t scroll{lv_obj_get_scroll_x(object), lv_obj_get_scroll_y(object)};
        bool floating = lv_obj_is_floating(object);
#if LVSERIAL_USE_ADVANCED_KEYBOARD
        espos::keyboard::authored_keyboard_view(object, position, scroll, floating);
#endif
        result["coords"] = Area::ToJSON(position.x, position.y,
                                        lv_obj_get_width(object), lv_obj_get_height(object));
        result["click"] = lv_obj_is_clickable(object);
        result["hidden"] = lv_obj_is_hidden(object);
        result["parevent"] = lv_obj_is_event_bubble(object);
        result["nativeStyles"] = NativeStyles::Read(object);
        result["flags"] = NativeFlags::Read(object);
        result["flags"]["floating"] = floating;
        result["scroll"] = {{"enabled", lv_obj_is_scrollable(object)}, {"direction", lv_obj_get_scroll_dir(object)},
            {"scrollbar", lv_obj_get_scrollbar_mode(object)}, {"snapX", lv_obj_get_scroll_snap_x(object)},
            {"snapY", lv_obj_get_scroll_snap_y(object)}, {"x", scroll.x}, {"y", scroll.y},
            {"elastic", lv_obj_is_scroll_elastic(object)}, {"momentum", lv_obj_is_scroll_momentum(object)},
            {"chainHorizontal", lv_obj_is_scroll_chain_hor(object)}, {"chainVertical", lv_obj_is_scroll_chain_ver(object)}};
        result["checked"] = lv_obj_has_state(object, LV_STATE_CHECKED);
        result["disabled"] = lv_obj_has_state(object, LV_STATE_DISABLED);
        const auto opacity = lv_obj_get_style_opa(object, LV_PART_MAIN);
        if (opacity != LV_OPA_COVER || !result.contains("opascale"))
            result["opascale"] = opacity;
        result["opascaleen"] = opacity != LV_OPA_COVER;
        if (!result.contains("style"))
            result["style"] = Style::Read(object, LV_PART_MAIN);
        for (const auto* name : {"drag", "dragThrow", "dragParent", "top"})
            if (!result.contains(name)) result[name] = false;
        if (!result.contains("dragDir"))
            result["dragDir"] = 3;
        if (!result.contains("protect"))
            result["protect"] = {{"protNone", true}, {"protChildChange", false}, {"protParent", false},
                {"protPos", false}, {"protFollow", false}, {"protPressLost", false}, {"protClickFocus", false}};
        return result;
    }

    lv_obj_t* LVObject::FromJSON(json j, lv_obj_t* widget)
    {
        if (!j.is_object())
            return widget;
        if (!widget)
            widget = lv_obj_create(lv_screen_active());

        auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(widget));
        if (!data)
        {
            data = new ObjectUserData{};
            lv_obj_set_user_data(widget, data);
            lv_obj_add_event_cb(widget, FreeObjectUserData, LV_EVENT_DELETE, data);
        }
        data->objectJson["base"] = j;
        if (j["name"].is_string())
            data->objectName = j["name"].get<std::string>();

        // Modern snapshots store explicit styles, including their state/part
        // selectors. Legacy resolved styles also contain inherited theme values;
        // applying both turns those inherited values into local overrides.
        if (!HasNativeStyleSnapshot(j)) ApplyBaseStyle(widget, ReadObject(j, "style"));
        const auto& coords = ReadObject(j, "coords");
        for (const auto* property : {"x", "y", "width", "height"})
        {
            const auto value = coords.find(property);
            if (value != coords.end() && value->is_number_integer())
                SetValue(widget, std::string("/base/coords/") + property, value->get<int>());
        }
        for (const auto* property : {"click", "hidden", "parevent"})
        {
            const auto value = j.find(property);
            if (value != j.end() && value->is_boolean())
                SetValue(widget, std::string("/base/") + property, value->get<bool>());
        }
        const auto enabled = j.find("opascaleen");
        const auto opacity = j.find("opascale");
        if (enabled != j.end() && opacity != j.end() && opacity->is_number_integer() &&
            ((enabled->is_boolean() && enabled->get<bool>()) ||
             (enabled->is_number_integer() && enabled->get<int>() != 0)))
            SetValue(widget, "/base/opascale", opacity->get<int>());

        NativeStyles::Apply(widget, j.value("nativeStyles", json::array()));
        if (j.contains("scroll") && j["scroll"].is_object())
        {
            const auto& scroll = j["scroll"];
            lv_obj_set_scrollable(widget, scroll.value("enabled", true));
            lv_obj_set_scroll_dir(widget, static_cast<lv_dir_t>(scroll.value("direction", int(LV_DIR_ALL))));
            lv_obj_set_scrollbar_mode(widget, static_cast<lv_scrollbar_mode_t>(scroll.value("scrollbar", int(LV_SCROLLBAR_MODE_AUTO))));
            lv_obj_set_scroll_snap_x(widget, static_cast<lv_scroll_snap_t>(scroll.value("snapX", 0)));
            lv_obj_set_scroll_snap_y(widget, static_cast<lv_scroll_snap_t>(scroll.value("snapY", 0)));
            lv_obj_set_scroll_elastic(widget, scroll.value("elastic", true));
            lv_obj_set_scroll_momentum(widget, scroll.value("momentum", true));
            lv_obj_set_scroll_chain_hor(widget, scroll.value("chainHorizontal", true));
            lv_obj_set_scroll_chain_ver(widget, scroll.value("chainVertical", true));
            lv_obj_scroll_to(widget, scroll.value("x", 0), scroll.value("y", 0), LV_ANIM_OFF);
        }
        if (j.contains("checked")) lv_obj_set_state(widget, LV_STATE_CHECKED, j.value("checked", false));
        if (j.contains("disabled")) lv_obj_set_state(widget, LV_STATE_DISABLED, j.value("disabled", false));
        NativeFlags::Apply(widget, j.value("flags", json::object()));
        if(j.contains("bootstrapAction") && j["bootstrapAction"].is_string())
            BootstrapActions::Bind(widget, j["bootstrapAction"].get<std::string>());
        return widget;
    }

    bool LVObject::SetStyle(lv_obj_t* obj, std::string key, std::string value,
                            std::map<std::string, lv_style_t*> stylePtrs)
    {
        const auto style = stylePtrs.find(value);
        if (key != "/base/style" || style == stylePtrs.end() || !style->second)
            return false;
        lv_obj_add_style(obj, style->second, LV_PART_MAIN);
        if (auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(obj)))
            data->objectJson["base"]["style"] = Style::ToJSON(*style->second, value);
        return true;
    }

    bool LVObject::SetValue(lv_obj_t* obj, std::string key, int value)
    {
        if (key == "/base/coords/x")
            lv_obj_set_x(obj, value);
        else if (key == "/base/coords/y")
            lv_obj_set_y(obj, value);
        else if (key == "/base/coords/width")
            lv_obj_set_width(obj, value);
        else if (key == "/base/coords/height")
            lv_obj_set_height(obj, value);
        else if (key == "/base/click")
            lv_obj_set_clickable(obj, value != 0);
        else if (key == "/base/hidden")
            lv_obj_set_hidden(obj, value != 0);
        else if (key == "/base/parevent")
            lv_obj_set_event_bubble(obj, value != 0);
        else if (key == "/base/opascale")
            lv_obj_set_style_opa(obj, std::clamp(value, 0, static_cast<int>(LV_OPA_COVER)), LV_PART_MAIN);
        else if (key == "/base/top")
        {
            if (value)
                lv_obj_move_foreground(obj);
        }
        else if (key == "/base/opascaleen")
        {
            const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(obj));
            const auto opacity = data && data->objectJson.contains("base") ?
                data->objectJson["base"].value("opascale", 255) : 255;
            lv_obj_set_style_opa(obj, value ? opacity : 255, LV_PART_MAIN);
        }
        else if (key != "/base/drag" && key != "/base/dragDir" && key != "/base/dragParent" &&
                 key != "/base/dragThrow" && key.compare(0, 14, "/base/protect/") != 0)
            return false;
        auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(obj));
        if (data)
        {
            const bool boolean = key == "/base/drag" || key == "/base/dragParent" || key == "/base/dragThrow" ||
                key == "/base/top" || key == "/base/opascaleen" || key.compare(0, 14, "/base/protect/") == 0;
            if (boolean)
                data->objectJson[json::json_pointer(key)] = value != 0;
            else
                data->objectJson[json::json_pointer(key)] = value;
        }
        return true;
    }
}
#else

namespace Serialization
{
    json LVObject::ToJSON(lv_obj_t* object)
    {
        json j;
        ObjectUserData* objectData = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
        j["name"] = objectData ? objectData->objectName : "";
        int x = lv_obj_get_x(object);
        int y = lv_obj_get_y(object);
        int width = lv_obj_get_width(object);
        int height = lv_obj_get_height(object);
        j["coords"] = Area::ToJSON(
            x,
            y,
            width,
            height
        );
        // We have to process children in JsonDoc
        if (object->click == 1)
            j["click"] = true;
        else
            j["click"] = false;

        if (object->drag == 1)
            j["drag"] = true;
        else
            j["drag"] = false;

        j["dragDir"] = static_cast<uint8_t>(object->drag_dir);
        j["dragParent"] = static_cast<uint8_t>(object->drag_parent);
        if (object->drag_throw == 1)
            j["dragThrow"] = true;
        else
            j["dragThrow"] = false;
#if LV_USE_EXT_CLICK_AREA == LV_EXT_CLICK_AREA_FULL
		j["extClkPad"] = Area::ToJSON(object->ext_click_pad);
#endif
        j["extDrwPad"] = static_cast<uint16_t>(object->ext_draw_pad);
        if (object->hidden == 1)
            j["hidden"] = true;
        else
            j["hidden"] = false;
        j["opascale"] = static_cast<uint8_t>(object->opa_scale);
        if (object->opa_scale_en == 1)
            j["opascaleen"] = true;
        else
            j["opascaleen"] = false;

        if (object->parent_event == 1)
            j["parevent"] = true;
        else
            j["parevent"] = false;
        j["protect"] = object->protect;

#ifdef LV_USE_OBJ_REALIGN
        j["realign"] = Realign::ToJSON(object->realign);
#endif
        lv_style_t* sp = const_cast<lv_style_t*>(object->style_p);
        if (sp != nullptr)
            j["style"] = Style::ToJSON(*sp);

        if (object->top == 1)
            j["top"] = true;
        else
            j["top"] = false;
        uint8_t protect = lv_obj_get_protect(object);
        bool none, childChg, parent, pos, follow, pressLost, clickFocus;
        none = childChg = parent = pos = follow = pressLost = clickFocus = false;
        if (protect == 0)
        {
            none = true;
        }
        else
        {
            if (protect && true)
                childChg = true;
            if (protect && true)
                parent = true;
            if (protect && true)
                pos = true;
            if (protect && true)
                follow = true;
            if (protect && true)
                pressLost = true;
            if (protect && true)
                clickFocus = true;
        }
        j["protect"] = nullptr;
        j["protect"]["protNone"] = none;
        j["protect"]["protChildChange"] = childChg;
        j["protect"]["protParent"] = parent;
        j["protect"]["protPos"] = pos;
        j["protect"]["protFollow"] = follow;
        j["protect"]["protPressLost"] = pressLost;
        j["protect"]["protClickFocus"] = clickFocus;

        return j;
    }

    lv_obj_t* LVObject::FromJSON(json j, lv_obj_t* widget)
    {
        if (widget == nullptr)
        {
            widget = lv_obj_create(lv_disp_get_scr_act(nullptr), nullptr);
        }
        if (j["style"].is_object())
        {
            lv_style_t style;
            style = Style::FromJSON(j["style"]);
            widget->style_p = &style;
        }
        if (j["coords"].is_object())
        {
            sAreaPos area = Area::FromJSON(j["coords"]);
            lv_obj_set_x(widget, area.x);
            lv_obj_set_y(widget, area.y);
            lv_obj_set_width(widget, area.width);
            lv_obj_set_height(widget, area.height);
        }

        if (j["click"].is_boolean())
        {
            if (j["click"] == true)
            {
                widget->click = 1;
            }
            else
            {
                widget->click = 0;
            }
        }
        else
        {
            widget->click = 0;
        }

        if (j["drag"].is_boolean())
        {
            if (j["drag"] == true)
            {
                widget->drag = 1;
            }
            else
            {
                widget->drag = 0;
            }
        }
        else
        {
            widget->drag = 0;
        }
        if (j["dragDir"].is_number())
        {
            widget->drag_dir = j["dragDir"].get<uint8_t>();
        }
        else
        {
            widget->drag_dir = 0;
        }
        if (j["dragParent"].is_number())
        {
            widget->drag_parent = j["dragParent"].get<uint8_t>();
        }
        else
        {
            widget->drag_parent = 0;
        }
        if (j["dragThrow"].is_boolean())
        {
            if (j["dragThrow"])
                widget->drag_throw = 1;
            else
                widget->drag_throw = 0;
        }
        else
        {
            widget->drag_throw = 0;
        }
#if LV_USE_EXT_CLICK_AREA == LV_EXT_CLICK_AREA_FULL
		if (j["extClkPad"].is_object())
		{
			widget->ext_click_pad = Area::FromJSON(j["extClkPad"]);
		}
#endif
        if (j["extDrwPad"].is_number())
            widget->ext_draw_pad = j["extDrwPad"].get<uint16_t>();
        if (j["hidden"].is_boolean())
        {
            if (j["hidden"] == true)
                widget->hidden = 1;
            else
                widget->hidden = 0;
        }
        if (j["opascale"].is_number())
        {
            widget->opa_scale = j["opascale"].get<uint8_t>();
        }
        if (j["opascaleen"].is_number())
        {
            widget->opa_scale_en = j["opascaleen"].get<uint8_t>();
        }
        if (j["parevent"].is_boolean())
        {
            if (j["parevent"] == true)
                widget->parent_event = true;
            else
                widget->parent_event = false;
        }
        if (j["protect"].is_number())
        {
            widget->protect = j["protect"];
        }
#ifdef LV_USE_OBJ_REALIGN
        if (j["realign"].is_object())
        {
            widget->realign = Realign::FromJSON(j["realign"]);
        }
#endif
        if (j["top"].is_boolean())
        {
            if (j["top"] == true)
                widget->top = 1;
            else
                widget->top = 0;
        }

        bool none, childChg, parent, pos, follow, pressLost, clickFocus;
        none = childChg = parent = pos = follow = pressLost = clickFocus = false;
        none = j["protect"]["protNone"];
        childChg = j["protect"]["protChildChange"];
        parent = j["protect"]["protParent"];
        pos = j["protect"]["protPos"];
        follow = j["protect"]["protFollow"];
        pressLost = j["protect"]["protPressLost"];
        clickFocus = j["protect"]["protClickFocus"];
        uint8_t prot = 0;
        if (none)
        {
            prot = 0;
        }
        else
        {
            if (childChg)
                prot = prot | 0x01;
            if (parent)
                prot = prot | 0x02;
            if (pos)
                prot = prot | 0x04;
            if (follow)
                prot = prot | 0x08;
            if (pressLost)
                prot = prot | 0x10;
            if (clickFocus)
                prot = prot | 0x20;
        }
        lv_obj_set_protect(widget, prot);
        return widget;
    }

    bool LVObject::SetStyle(lv_obj_t* obj, std::string key, std::string value,
                            std::map<std::string, lv_style_t*> stylePtrs)
    {
        if (key == "/base/style")
        {
            if (stylePtrs.find(value) != stylePtrs.end())
            {
                lv_obj_set_style(obj, stylePtrs[value]);
                return true;
            }
        }

        return false;
    }

    bool LVObject::SetValue(lv_obj_t* obj, std::string key, int value)
    {
        if (key == "/base/coords/x")
        {
            lv_obj_set_x(obj, value);
            return true;
        }
        if (key == "/base/coords/width")
        {
            lv_obj_set_width(obj, value);
            return true;
        }
        if (key == "/base/coords/y")
        {
            lv_obj_set_y(obj, value);
            return true;
        }
        if (key == "/base/coords/height")
        {
            lv_obj_set_height(obj, value);
            return true;
        }
        if (key == "/base/dragDir")
        {
            obj->drag_dir = value;
            return true;
        }
#if LV_USE_EXT_CLICK_AREA == LV_EXT_CLICK_AREA_FULL
		if (key == "/base/extClkPad/x1")
		{
			lv_area_t a = obj->ext_click_pad;
			a.x1 = value;
			obj->ext_click_pad = a;
			return true;
		}
		if (key == "/base/extClkPad/x2")
		{
			lv_area_t a = obj->ext_click_pad;
			a.x2 = value;
			obj->ext_click_pad = a;
			return true;
		}
		if (key == "/base/extClkPad/y1")
		{
			lv_area_t a = obj->ext_click_pad;
			a.y1 = value;
			obj->ext_click_pad = a;
			return true;
		}
		if (key == "/base/extClkPad/y2")
		{
			lv_area_t a = obj->ext_click_pad;
			a.y2 = value;
			obj->ext_click_pad = a;
			return true;
		}
#endif
        if (key == "/base/extDrawPad")
        {
            obj->ext_draw_pad = value;
            return true;
        }
        if (key == "/base/opascale")
        {
            obj->opa_scale = value;
            return true;
        }
#ifdef LV_USE_OBJ_REALIGN
        if (key == "/base/realign/xofs")
        {
            obj->realign.xofs = value;
            return true;
        }
        if (key == "/base/realign/yofs")
        {
            obj->realign.yofs = value;
            return true;
        }
        if (key == "/base/realign/align")
        {
            obj->realign.align = value;
            return true;
        }
        if (key == "/base/click")
        {
            obj->click = value;
            return true;
        }
        if (key == "/base/drag")
        {
            obj->drag = value;
            return true;
        }
        if (key == "/base/dragParent")
        {
            obj->drag_parent = value;
            return true;
        }
        if (key == "/base/dragThrow")
        {
            obj->drag_throw = value;
            return true;
        }
        if (key == "/base/hidden")
        {
            obj->hidden = value;
            return true;
        }
        if (key == "/base/opascaleen")
        {
            obj->opa_scale_en = value;
            return true;
        }
        if (key == "/base/parevent")
        {
            obj->parent_event = value;
            return true;
        }

#ifdef LV_USE_OBJ_REALIGN
        if (key == "/base/realign/xofs")
        {
            obj->realign.xofs = value;
            return true;
        }
        if (key == "/base/realign/yofs")
        {
            obj->realign.yofs = value;
            return true;
        }
        if (key == "/base/realign/align")
        {
            obj->realign.align = value;
            return true;
        }
        if (key == "/base/realign/orthoalign")
        {
            obj->realign.origo_align = value;
            return true;
        }

        if (key == "/base/realign/autore")
        {
            obj->realign.auto_realign = value;
            return true;
        }
#endif
        if (key == "/base/top")
        {
            obj->top = value;
            return true;
        }

        if (key == "/base/protect/protNone")
        {
            obj->protect = 0;
            return true;
        }
        if (key == "/base/protect/protChildChange")
        {
            obj->protect = obj->protect | 0x01;
            return true;
        }
        if (key == "/base/protect/protParent")
        {
            obj->protect = obj->protect | 0x02;
            return true;
        }
        if (key == "/base/protect/protPos")
        {
            obj->protect = obj->protect | 0x04;
            return true;
        }
        if (key == "/base/protect/protFollow")
        {
            obj->protect = obj->protect | 0x08;
            return true;
        }
        if (key == "/base/protect/pressLost")
        {
            obj->protect = obj->protect | 0x10;
            return true;
        }
        if (key == "/base/protect/protClickFocus")
        {
            obj->protect = obj->protect | 0x20;
            return true;
        }

        return false;
    }
#endif
}
#endif
