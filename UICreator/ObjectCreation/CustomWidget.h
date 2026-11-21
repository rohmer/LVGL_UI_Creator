#pragma once
#include <Serialization/CustomWidgets.h>

namespace CustomWidget
{
#if LVGL_VERSION_MAJOR >= 9
    inline lv_obj_t* Create(int id, lv_obj_t* parent, int x, int y, std::string& name)
    {
        if (id != 30 && id != 31) return nullptr;
        name = id == 30 ? "Text box" : "Layer view";
        auto* object = Serialization::CustomWidgets::Create(id == 30 ? "textBox" : "layerView", parent);
        lv_obj_set_pos(object, x, y);
        return object;
    }
#endif
}
