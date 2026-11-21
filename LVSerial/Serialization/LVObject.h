#pragma once
#include <any>
#include <string>
#include <map>
#include "../../3rdParty/JSON/json.hpp"
#include <lvgl/lvgl.h>
#if LVGL_VERSION_MAJOR < 9 && !defined(LV_RUNTIME_PORTABLE)
#include "Style.h"
#include "Realign.h"
#endif
#include "Area.h"
#include "ObjectUserData.h"

using json = nlohmann::json;

namespace Serialization
{
    inline bool HasNativeStyleSnapshot(const json& base)
    {
        const auto styles = base.find("nativeStyles");
        return styles != base.end() && styles->is_array();
    }
    class LVObject
    {
    public:
        static json ToJSON(lv_obj_t* obj);
        static lv_obj_t* FromJSON(json j, lv_obj_t* widget = nullptr);
        static bool SetValue(lv_obj_t* obj, std::string key, int value);
        static bool SetStyle(lv_obj_t* obj, std::string key, std::string value,
                             std::map<std::string, lv_style_t*> stylePtrs);
    };
}
