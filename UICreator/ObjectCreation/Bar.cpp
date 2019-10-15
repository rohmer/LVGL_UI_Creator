#include "Bar.h"

lv_obj_t* Bar::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent or parentTreeNode
    if (parent == nullptr)
        return nullptr;

    lv_obj_t* newObj = lv_bar_create(parent, nullptr);
    lv_obj_set_size(newObj, 50, 20);
    lv_obj_set_parent(newObj, parent);
    lv_bar_set_range(newObj, 0, 100);
    lv_bar_set_value(newObj, 50, LV_ANIM_OFF);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
