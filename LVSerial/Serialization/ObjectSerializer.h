#pragma once

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include "../../3rdParty/LVGL/lvgl/lvgl.h"

#include "LVObject.h"

#include "LVArc.h"
#include "LVBar.h"
#include "LVButton.h"
#include "LVButtonMatrix.h"
#include "LVCalendar.h"
#include "LVCanvas.h"
#include "LVCheckBox.h"
#include "LVFont.h"
#include "LVImage.h"
#include "LVLabel.h"

namespace Serialization
{
    //
    // Serializes an object to its proper JSON Class based on its Widget Type
    class ObjectSerializer
    {
    public:
        static json SerializeObject(lv_obj_t* obj);
    };
}