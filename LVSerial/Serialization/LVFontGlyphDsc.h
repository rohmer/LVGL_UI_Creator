#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

using json = nlohmann::json;

namespace Serialization
{
	class LVFontGlyphDsc
	{
	public:
		static json ToJSON(const lv_font_fmt_txt_glyph_dsc_t &glyphDsc);
		static lv_font_fmt_txt_glyph_dsc_t* FromJSON(json j);

	};
}