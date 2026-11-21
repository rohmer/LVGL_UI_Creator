#pragma once
#include "../../Utils/EditorStyle.h"
#include "AdvancedProperties.h"
#include <Serialization/CustomWidgets.h>

#if LVGL_VERSION_MAJOR >= 9
class CustomProperties
{
public:
    static bool Supports(lv_obj_t* object) { return Serialization::CustomWidgets::Type(object) != nullptr; }
    static void Update(PropertyWindow* window, const json& record) { AdvancedProperties::Update(window, record); }
};
#endif
