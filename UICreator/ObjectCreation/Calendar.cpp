#include "Calendar.h"

lv_obj_t* Calendar::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent
    if (parent == nullptr)
        return nullptr;
    lv_obj_t* newObj = lv_calendar_create(parent, nullptr);
    lv_obj_set_size(newObj, 150, 150);
    lv_obj_set_parent(newObj, parent);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
