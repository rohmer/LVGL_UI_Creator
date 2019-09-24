#include "LVArc.h"
namespace Serialization
{
	json LVArc::ToJSON(lv_obj_t* arc)
	{
		json j;
		j["base"] = LVObject::ToJSON(arc);
		j["arc"]["angle"]["start"] = lv_arc_get_angle_start(arc);
		j["arc"]["angle"]["end"] = lv_arc_get_angle_end(arc);
		static const lv_style_t *style = lv_arc_get_style(arc, LV_ARC_STYLE_MAIN);
		bool rounded = false;
		if (style->line.rounded == 1)
			rounded = true;
		j["arc"]["style"]["line.rounded"] = rounded;
		j["arc"]["style"]["line.width"] = style->line.width;
		j["arc"]["style"]["line.color"] = style->line.color.full;
		j["arc"]["style"]["line.opa"] = style->line.opa;
		return j;
	}

	lv_obj_t* LVArc::FromJSON(json j)
	{
		lv_obj_t* arc = lv_obj_create(lv_scr_act(), nullptr);
		if (j["base"].is_object())
		{
			arc = LVObject::FromJSON(j["base"], arc);
		}
		else
		{
			// No base, no object
			return nullptr;
		}

		if (j["arc"]["angle"].is_object())
		{
			int start=0, end=0;
			if (j["arc"]["angle"]["start"].is_number())
				start = j["arc"]["angle"]["start"].get<int>();
			if (j["arc"]["angle"]["end"].is_number())
				end = j["arc"]["angle"]["end"].get<int>();
			lv_arc_set_angles(arc, start, end);
		}

		if (j["arc"]["style"].is_object())
		{
			int rounded = 0, lineWidth = 0, color=0, opa=255;
			if (j["arc"]["style"]["line.rounded"].is_boolean() && 
				j["arc"]["style"]["line.rounded"].get<bool>())
				rounded = 1;
			if (j["arc"]["style"]["line.width"].is_number())
				lineWidth = j["arc"]["style"]["line.width"].get<int>();
			if (j["arc"]["style"]["line.color"].is_number())
				color = j["arc"]["style"]["line.color"].get<int>();
			if (j["arc"]["style"]["line.opa"].is_number())
				opa = j["arc"]["style"]["line.opa"];
			static lv_style_t style;
			lv_style_copy(&style, &lv_style_plain);
			style.line.color.full = color;
			style.line.rounded = rounded;
			style.line.width = lineWidth;
			style.line.opa = opa;
			lv_arc_set_style(arc, LV_ARC_STYLE_MAIN, &style);
		}

		return arc;
	}
}