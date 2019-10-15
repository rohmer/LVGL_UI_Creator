#include "CheckBox.h"

lv_obj_t* CheckBox::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent
    if (parent == nullptr)
        return nullptr;
    lv_obj_t* newObj = lv_cb_create(parent, nullptr);
    lv_obj_set_size(newObj, 80, 30);
    lv_cb_set_text(newObj, "Text");
    lv_cb_set_checked(newObj, true);
    lv_obj_set_parent(newObj, parent);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
