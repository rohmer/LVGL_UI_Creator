#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

using json = nlohmann::json;

namespace Serialization
{
	class Realign
	{
	public:
		static lv_reailgn_t FromJSON(json j)
		{
			lv_reailgn_t r;

			r.xofs = j["xofs"].get<uint16_t>();
			r.yofs = j["yofs"].get<uint16_t>();
			r.align = j["align"].get<uint8_t>();
			if (j["autore"] == true)
				r.auto_realign = 1;
			else
				r.auto_realign = 0;
			if (j["origoalign"] == true)
				r.origo_align = 1;
			else
				r.origo_align = 0;
			return r;
		}

		static json ToJSON(lv_reailgn_t realign)
		{
			json j;
			j["xofs"] = realign.xofs;
			j["yofs"] = realign.yofs;
			j["align"] = realign.align;
			if (realign.auto_realign == 1)
				j["autore"] = true;
			else
				j["autore"] = false;

			if (realign.origo_align == 1)
				j["origoalign"] = true;
			else
				j["origoalign"] = false;
			return j;
		}
	};
}