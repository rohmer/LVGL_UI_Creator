#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

using json = nlohmann::json;

namespace Serialization
{
	class Area
	{
	public:		
		static lv_area_t FromJSON(json j)
		{
			lv_area_t area;

			area.x1 = (int)j["x1"];
			area.x2 = (int)j["x2"];
			area.y1 = (int)j["y1"];
			area.y2 = (int)j["y2"];
			return area;
		}

		static json ToJSON(int x1, int y1, int x2, int y2)
		{
			json j;
            j["x1"] =x1;
			j["y1"] = y1;
			j["x2"] = x2;
			j["y2"] = y2;
			return j;
		}
		int X1, Y1, X2, Y2;
	};
}