#pragma once

#include <cstdlib>
#include <sstream>
#include <string>
#include <time.h>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"
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
    };
}
