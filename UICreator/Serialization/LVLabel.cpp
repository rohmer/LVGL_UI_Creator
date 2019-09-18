#include "LVLabel.h"

namespace Serialization
{
	json LVLabel::ToJSON(lv_obj_t* label)
	{
		json j;
		j["base"] = LVObject::ToJSON(label);
		std::string text(lv_label_get_text(label));
		j["text"] = text;
		j["longmode"] = (uint8_t)lv_label_get_long_mode(label);
		j["textalign"] = (uint8_t)lv_label_get_align(label);		
		j["drawback"] = lv_label_get_body_draw(label);
		return j;
	}

	lv_obj_t* LVLabel::FromJSON(json j)
	{
		lv_obj_t* label;
		if (j["base"].is_object())
		{
			label = LVObject::FromJSON(j["base"]);
		}
		else
		{
			// No base, no object
			return nullptr;
		}
		
		if (j["text"].is_string())
		{
			lv_label_set_text(label, j["text"].get<std::string>().c_str());
		}

		if (j["longmode"].is_number())
		{
			lv_label_set_long_mode(label,j["longmode"].get<uint8_t>());
		}
		if (j["textalign"].is_number())
		{
			lv_label_set_align(label,j["textalign"].get<uint8_t>());
		}
		if (j["drawback"].is_number())
		{
			lv_label_set_body_draw(label, j["drawback"].get<bool>());
		}

		return label;
	}

}