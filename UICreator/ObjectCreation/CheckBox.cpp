#include "CheckBox.h"

lv_obj_t* CheckBox::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent
    if (parent == nullptr)
        return nullptr;
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t* newObj = lv_checkbox_create(parent);
    lv_checkbox_set_text(newObj, "Text");
    lv_obj_add_state(newObj, LV_STATE_CHECKED);
#else
    lv_obj_t* newObj = lv_cb_create(parent, nullptr);
    lv_cb_set_text(newObj, "Text");
    lv_cb_set_checked(newObj, true);
#endif
    lv_obj_set_size(newObj, 80, 30);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
