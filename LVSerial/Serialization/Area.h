#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

using json = nlohmann::json;

namespace Serialization
{
    struct sAreaPos
    {
        int x, y, height, width;
    };
	class Area
	{
	public:		
		static sAreaPos FromJSON(json j)
		{
			sAreaPos area;

			area.x = (int)j["x"];
			area.width = (int)j["width"];
			area.y = (int)j["y"];
			area.height = (int)j["height"];
			return area;
		}

		static json ToJSON(int x, int y, int width, int height)
		{
			json j;
            j["x"] =x;
			j["y"] = y;
			j["width"] = width;
			j["height"] = height;
			return j;
		}
		int X1, Y1, Width, Height;
	};
}