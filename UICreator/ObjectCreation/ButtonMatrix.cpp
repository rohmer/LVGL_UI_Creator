#include "ButtonMatrix.h"

lv_obj_t* ButtonMatrix::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent or parentTreeNode
    if (parent == nullptr)
        return nullptr;
    lv_obj_t* newObj = lv_btnm_create(parent, nullptr);
    lv_obj_set_size(newObj, 80, 80);
    lv_obj_set_parent(newObj, parent);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
