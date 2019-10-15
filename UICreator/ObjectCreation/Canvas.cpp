#include "Canvas.h"

lv_obj_t* Canvas::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent
    if (parent == nullptr)
        return nullptr;
    lv_obj_t* newObj = lv_canvas_create(parent, nullptr);
    lv_obj_set_size(newObj, 50, 50);
    lv_obj_set_parent(newObj, parent);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
