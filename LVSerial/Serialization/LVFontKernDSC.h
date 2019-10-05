#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"


using json = nlohmann::json;

namespace Serialization
{
	class LVFontKernDSC
	{
	public:
		static json ToJSON(lv_font_fmt_txt_dsc_t &fontDsc, int kernClasses);
		static lv_font_fmt_txt_dsc_t FromJSON(lv_font_fmt_txt_dsc_t& fontDsc, int kernClasses, json j);

	};
}