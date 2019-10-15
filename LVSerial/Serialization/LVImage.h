#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"
#include "Base64.h"

#include "LVObject.h"
using json = nlohmann::json;

namespace Serialization
{
    class LVImage
    {
    public:
        static json ToJSON(lv_obj_t* img);
        static lv_obj_t* FromJSON(json j);
        static bool SetValue(lv_obj_t* obj, std::string key, int value);
    };
}
