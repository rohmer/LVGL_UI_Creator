#pragma once

#include <cstdlib>
#include <sstream>
#include <string>
#include <time.h>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

using json = nlohmann::json;

namespace Serialization
{
	class Style
	{
	public:
		static json Serialize(lv_style_t &style);
		static json Serialize(lv_style_t &style, std::string name);
		static lv_style_t Deserialize(json j);

	private:
		static bool styleComp(lv_style_t &st1, lv_style_t &st2);
	};
}