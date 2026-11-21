#pragma once
#include "Protobuf.h"

#include <string>

#include "../../3rdParty/JSON/json.hpp"
#include <lvgl/lvgl.h>

#include "LVObject.h"
#include "RuntimeFeatures.h"

#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_ARC
#include "LVArc.h"
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BAR
#include "LVBar.h"
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTON
#include "LVButton.h"
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_BUTTONMATRIX
#include "LVButtonMatrix.h"
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CALENDAR
#include "LVCalendar.h"
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CANVAS
#include "LVCanvas.h"
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CHECKBOX
#include "LVCheckBox.h"
#endif
#include "LVFont.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_IMAGE
#include "LVImage.h"
#endif
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_LABEL
#include "LVLabel.h"
#endif

namespace Serialization
{
    //
    // Serializes an object to its proper JSON Class based on its Widget Type
    class ObjectSerializer
    {
    public:
        static const char* TypeName(lv_obj_t* object);
        static std::vector<std::string> SupportedTypes();
        static lv_obj_t* Create(const std::string& type, lv_obj_t* parent = nullptr);
        static Binary ToProtobuf(lv_obj_t* object);
        static lv_obj_t* FromProtobuf(const Binary& record);
        static json SerializeObject(lv_obj_t* obj);
        static lv_obj_t* DeserializeObject(json record);
    };
}
