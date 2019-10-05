#pragma once
#include <any>
#include <string>
#include <map>
#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

#include "Style.h"
#include "Area.h"
#include "Realign.h"

using json = nlohmann::json;

namespace Serialization
{
	class LVObject
	{
	public:
		static json ToJSON(lv_obj_t* obj);
		static lv_obj_t* FromJSON(json j, lv_obj_t* widget = nullptr);
		static bool SetValue(lv_obj_t *obj, std::string key, int value);		

	};


}