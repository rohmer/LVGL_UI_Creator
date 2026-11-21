#include "Button.h"
#include <Serialization/LVButton.h>

lv_obj_t* Button::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent or parentTreeNode
    if (parent == nullptr)
        return nullptr;
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t* newObj = lv_button_create(parent);
    Serialization::LVButton::SetText(newObj, "Button");
#else
    lv_obj_t* newObj = lv_btn_create(parent, nullptr);
#endif
    lv_obj_set_size(newObj, 90, 36);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
