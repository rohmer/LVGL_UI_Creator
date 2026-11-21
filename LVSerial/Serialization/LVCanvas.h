#pragma once

#include "../../3rdParty/JSON/json.hpp"
#include <lvgl/lvgl.h>

#include "Style.h"
#include "LVObject.h"
#include "Base64.h"

using json = nlohmann::json;

namespace Serialization
{
    class LVCanvas
    {
    public:
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        static void Apply(lv_obj_t* canvas, const json& record);
#endif
        static json ToJSON(lv_obj_t* canvas);
        static lv_obj_t* FromJSON(json j);
        static bool SetValue(lv_obj_t* obj, std::string key, int value);
        static bool SetValue(lv_obj_t* obj, std::string key, lv_style_t style);
        static bool SetValue(lv_obj_t* obj,
                             uint32_t x,
                             uint32_t y,
                             uint32_t cf,
                             std::vector<unsigned char> img,
                             lv_style_t style);
    };
}
