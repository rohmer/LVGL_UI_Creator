#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"
using json = nlohmann::json;

namespace Serialization
{
	class LVFont
	{
		static json ToJSON(lv_font_t &font);
		static lv_font_t &FromJSON(json j);

	};
}