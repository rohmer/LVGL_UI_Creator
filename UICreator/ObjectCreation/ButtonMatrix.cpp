#include "ButtonMatrix.h"

lv_obj_t* ButtonMatrix::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent or parentTreeNode
    if (parent == nullptr)
        return nullptr;
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t* newObj = lv_buttonmatrix_create(parent);
#else
    lv_obj_t* newObj = lv_btnm_create(parent, nullptr);
#endif
    lv_obj_set_size(newObj, 180, 100);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
