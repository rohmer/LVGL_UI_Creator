#include "LVObject.h"

namespace Serialization
{
	json LVObject::ToJSON(lv_obj_t *object)
	{
		json j;

		j["coords"] = Area::ToJSON(object->coords);
		// We have to process children in JsonDoc
		if (object->click == 1)
			j["click"] = true;
		else
			j["click"] = false;

		if (object->drag == 1)
			j["drag"] = true;
		else
			j["drag"] = false;

		j["dragDir"] = (uint8_t)object->drag_dir;
		j["dragParent"] = (uint8_t)object->drag_parent;
		if (object->drag_throw == 1)
			j["dragThrow"] = true;
		else
			j["dragThrow"] = false;
#if LV_USE_EXT_CLICK_AREA == LV_EXT_CLICK_AREA_FULL
		j["extClkPad"] = Area::ToJSON(object->ext_click_pad);
#endif
		j["extDrwPad"] = (uint16_t)object->ext_draw_pad;
		if (object->hidden == 1)
			j["hidden"] = true;
		else
			j["hidden"] = false;
		j["opascale"] = (uint8_t)object->opa_scale;
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
			j["style"] = Style::Serialize(*sp);

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
			if (protect && 0x01)
				childChg = true;
			if (protect && 0x02)
				parent = true;
			if (protect && 0x04)
				pos = true;
			if (protect && 0x08)
				follow = true;
			if (protect && 0x10)
				pressLost = true;
			if (protect && 0x20)
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
			style = Style::Deserialize(j["style"]);
			widget->style_p = &style;
		}
		if (j["coords"].is_object())
		{
			widget->coords = Area::FromJSON(j["coords"]);
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

	bool LVObject::SetValue(lv_obj_t* obj, std::string key, int value)
	{
		if (key == "/base/coords/x1")
		{
			lv_area_t a = obj->coords;
			a.x1 = value;
			obj->coords = a;
			return true;
		}
		if (key == "/base/coords/x2")
		{
			lv_area_t a = obj->coords;
			a.x2 = value;
			obj->coords = a;
			return true;
		}
		if (key == "/base/coords/y1")
		{
			lv_area_t a = obj->coords;
			a.y1 = value;
			obj->coords = a;
			return true;
		}
		if (key == "/base/coords/y2")
		{
			lv_area_t a = obj->coords;
			a.y2 = value;
			obj->coords = a;
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
		if(key=="/base/realign/xofs")
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
			obj->realign.auto_realign= value;
			return true;
		}
#endif
		if (key == "/base/top")
		{
			obj->top = value;
			return true;
		}

		if(key=="/base/protect/protNone")
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