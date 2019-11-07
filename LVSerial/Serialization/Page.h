#pragma once

#include <map>
#include <string>
#include <vector>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

#pragma region Type Includes
#include "LVArc.h"
#include "LVBar.h"
#include "LVButton.h"
#include "LVButtonMatrix.h"
#include "LVCalendar.h"
#include "LVCanvas.h"
#include "LVCheckBox.h"
#include "LVFont.h"
#pragma endregion

using json = nlohmann::json;

// Contains everything in the document, calls out to the other serializers to do the serializing
// This allows us to read everything, and then assign parents after
namespace Serialization
{
    class Page
    {
    public:
        static std::vector<lv_obj_t*> FromJSON(json j);
        static json ToJSON(std::vector<lv_obj_t*> objects);

    private:
        static std::map<unsigned int, lv_obj_t*> idToObj;
        static json serializeObject(lv_obj_t* object);
        static std::map<std::string, unsigned int> objectNames;
        static std::map<const lv_style_t*, std::string> styles;
        static std::map <std::string, const lv_style_t*> nameToStyle;

        static void getStyles(lv_obj_t* object);
        static void addStyle(std::string styleName, const lv_style_t* style);
    };
}
