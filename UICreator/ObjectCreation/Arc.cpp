#include "Arc.h"

lv_obj_t* Arc::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent or parentTreeNode
    if (parent == nullptr)
        return nullptr;

    lv_obj_t* newObj = lv_arc_create(parent, nullptr);
    lv_obj_set_size(newObj, 50, 50);
    lv_obj_set_parent(newObj, parent);
    lv_arc_set_angles(newObj, 90, 270);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
