#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"
#include "LVFontCMap.h"
#include "LVFontGlyphDsc.h"
#include "LVFontKernDSC.h"

using json = nlohmann::json;

namespace Serialization
{
	class LVFontDsc
	{
	public:
		static json ToJSON(lv_font_fmt_txt_dsc_t &fontDsc, lv_font_t &font);
		static lv_font_fmt_txt_dsc_t* FromJSON(json j);

	};
}