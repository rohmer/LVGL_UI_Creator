#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include <lvgl/lvgl.h>

#include "Style.h"
#include "LVObject.h"

using json = nlohmann::json;

namespace Serialization
{
    class LVCalendar
    {
    public:
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        static void Apply(lv_obj_t* object, json record, bool applyStyles = true);
#endif
        static json ToJSON(lv_obj_t* label);
        static lv_obj_t* FromJSON(json j);
        static bool SetValue(lv_obj_t* obj, std::string key, int value);
        static bool SetValue(lv_obj_t* obj, std::string key, lv_style_t* style);
        static bool SetValue(lv_obj_t* obj, std::string key, std::vector<lv_calendar_date_t> value);
        static bool SetValue(lv_obj_t* obj, std::string key, std::vector<std::string> value);
    };
}
