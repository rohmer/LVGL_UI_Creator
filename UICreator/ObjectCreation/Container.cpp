#include "Container.h"

lv_obj_t* Container::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent
    if (parent == nullptr)
        return nullptr;
    lv_obj_t* newObj = lv_cont_create(parent, nullptr);
    lv_obj_set_size(newObj, 80, 80);
    lv_obj_set_parent(newObj, parent);
    lv_obj_set_pos(newObj, x, y);
    lv_cont_set_fit(newObj, LV_FIT_TIGHT);
    lv_cont_set_layout(newObj, LV_LAYOUT_PRETTY);
    return newObj;
}
