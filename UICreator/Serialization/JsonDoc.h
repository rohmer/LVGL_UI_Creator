#pragma once

#pragma once

#include <map>
#include <string>
#include <vector>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

using json = nlohmann::json;

// Contains everything in the document, calls out to the other serializers to do the serializing
// This allows us to read everything, and then assign parents after
namespace Serialization
{
	class JsonDoc
	{
	public:
		static std::vector<lv_obj_t*> FromJSON(json j);
		static json ToJSON(std::vector<lv_obj_t*> objects);
	};
}