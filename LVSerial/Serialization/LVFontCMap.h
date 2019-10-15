#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"
using json = nlohmann::json;

namespace Serialization
{
    class LVFontCMap
    {
    public:
        static json ToJSON(lv_font_fmt_txt_cmap_t& cmap);
        static lv_font_fmt_txt_cmap_t FromJSON(json j);
    };
}
