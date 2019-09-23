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
		j["style"] = Style::Serialize(*sp);

		if (object->top == 1)
			j["top"] = true;
		else
			j["top"] = false;
		
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
			widget->coords=Area::FromJSON(j["coords"]);
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
			if(j["hidden"]==true)
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
		return widget;
	}
}