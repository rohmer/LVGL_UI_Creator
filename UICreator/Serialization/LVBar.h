#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

#include "Style.h"
#include "LVObject.h"

using json = nlohmann::json;

namespace Serialization
{
	class LVBar
	{
	public:
		static json ToJSON(lv_obj_t* label);
		static lv_obj_t* FromJSON(json j);

	};
}