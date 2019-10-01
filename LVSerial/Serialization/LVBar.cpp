#include "LVBar.h"

namespace Serialization
{
	json LVBar::ToJSON(lv_obj_t* bar)
	{
		json j;
		j["base"] = LVObject::ToJSON(bar);

		j["bar"]["range"]["max"] = lv_bar_get_max_value(bar);
		j["bar"]["range"]["min"] = lv_bar_get_min_value(bar);
		j["bar"]["animT"] = lv_bar_get_anim_time(bar);
		j["bar"]["symmetrical"] = lv_bar_get_sym(bar);
		auto bgStyle = (lv_style_t*)lv_bar_get_style(bar, LV_BAR_STYLE_BG);
		auto inStyle = (lv_style_t*)lv_bar_get_style(bar, LV_BAR_STYLE_INDIC);
		j["bar"]["styleBG"] = Style::ToJSON(*bgStyle);
		j["bar"]["styleInd"] = Style::ToJSON(*inStyle);
		return j;
	}
	
	bool LVBar::SetValue(lv_obj_t* obj, std::string key, int value)
	{
		if(key=="/bar/range/max")
		{
			int min = lv_bar_get_min_value(obj);
			lv_bar_set_range(obj, min, value);
			return true;
		}
		if (key == "/bar/range/min")
		{
			int max = lv_bar_get_min_value(obj);
			lv_bar_set_range(obj, max, value);
			return true;
		}
		if (key == "/bar/animT")
		{
			lv_bar_set_anim_time(obj, value);
			return true;
		}
		if (key == "/bar/symmetrical")
		{
			lv_bar_set_sym(obj, value);
			return true;
		}
		return true;
	}

	bool LVBar::SetValue(lv_obj_t* obj, std::string key, lv_style_t *value)
	{
		if (key == "/bar/styleBG")
		{
			lv_bar_set_style(obj, LV_BAR_STYLE_BG, value);
			return true;
		}

		if (key == "/bar/styleInd")
		{
			lv_bar_set_style(obj, LV_BAR_STYLE_INDIC, value);
			return true;
		}
		return false;
	}
	
	lv_obj_t* LVBar::FromJSON(json j)
	{
		if (!j["base"].is_object())
			return nullptr;

		lv_obj_t *bar = lv_bar_create(lv_scr_act(), nullptr);
		bar = LVObject::FromJSON(j["base"], bar);

		if(j["bar"].is_object())
		{
			json barJ = j["bar"];
			if(barJ["range"].is_object())
			{
				int min = 0, max = 100;
				if(barJ["range"]["max"].is_number())
				{
					max = barJ["range"]["max"].get<int>();
				}
				if (barJ["range"]["min"].is_number())
				{
					min = barJ["range"]["min"].get<int>();
				}
				lv_bar_set_range(bar, min, max);
			}
			if(barJ["animT"].is_number())
			{
				lv_bar_set_anim_time(bar, barJ["animT"].get<int>());
			}
			if(barJ["styleBG"].is_object())
			{
				const lv_style_t* bgStyle = &Style::FromJSON(barJ["styleBG"]);
				lv_bar_set_style(bar, LV_BAR_STYLE_BG, bgStyle);
			}
			if (barJ["styleInd"].is_object())
			{
				const lv_style_t* inStyle = &Style::FromJSON(barJ["styleInd"]);
				lv_bar_set_style(bar, LV_BAR_STYLE_INDIC, inStyle);
			}
		}
		return bar;
	}

}
