#pragma once

#include <cstdlib>
#include <sstream>
#include <string>
#include <time.h>

#include "../../3rdParty/JSON/json.hpp"
#include <lvgl/lvgl.h>
#include "LVFont.h"

using json = nlohmann::json;

namespace Serialization
{
    class Style
    {
    public:
        static json ToJSON(lv_style_t& style);
        static json ToJSON(lv_style_t& style, std::string name);
        static const lv_style_t &FromJSON(json j);
        static bool StyleComp(lv_style_t st1, lv_style_t st2);
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
        static void Apply(lv_obj_t* object, const json& record, lv_style_selector_t selector);
        static json Read(lv_obj_t* object, lv_part_t part, json record = json::object());
#endif
    };
}
