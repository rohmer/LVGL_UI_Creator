#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

#include "LVObject.h"

namespace Serialization
{
    class LVCheckBox
    {
    public:
        static json ToJSON(lv_obj_t* cb);
        static lv_obj_t* FromJSON(json j);
        static bool SetValue(lv_obj_t* obj, std::string key, int value);
        static bool SetValue(lv_obj_t* obj, std::string key, std::string value);
    };
}
