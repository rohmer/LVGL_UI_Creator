#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

using json = nlohmann::json;

namespace Serialization
{
	class Style
	{
	public:		
		static json Serialize(lv_style_t &style);
		static lv_style_t Deserialize(json j);
	};
}