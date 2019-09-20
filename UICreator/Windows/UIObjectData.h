#pragma once

#include <lvgl/lvgl.h>
#include "../../3rdParty/JSON/json.hpp"

struct uiObjData
{
	const lv_style_t *style;
	nlohmann::json  *objectJSON;
};