#include "LVArc.h"
namespace Serialization
{
	json LVArc::ToJSON(lv_obj_t* arc)
	{
		json j;
		j["base"] = LVObject::ToJSON(arc);
		j["angle"]["start"] = lv_arc_get_angle_start(arc);
		j["angle"]["end"] = lv_arc_get_angle_end(arc);		
		return j;
	}

	lv_obj_t* LVArc::FromJSON(json j)
	{
		lv_obj_t* arc;
		if (j["base"].is_object())
		{
			arc = LVObject::FromJSON(j["base"]);
		}
		else
		{
			// No base, no object
			return nullptr;
		}

		if (j["angle"].is_object())
		{
			int start=0, end=0;
			if (j["angle"]["start"].is_number())
				start = j["angle"]["start"].get<int>();
			if (j["angle"]["end"].is_number())
				end = j["angle"]["end"].get<int>();
			lv_arc_set_angles(arc, start, end);
		}

		return arc;
	}
}