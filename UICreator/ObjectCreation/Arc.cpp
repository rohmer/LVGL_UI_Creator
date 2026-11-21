#include "Arc.h"
#include <Serialization/LVArc.h>

lv_obj_t* Arc::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent or parentTreeNode
    if (parent == nullptr)
        return nullptr;

    lv_obj_t* newObj = Serialization::LVArc::Create(parent);
    lv_obj_set_size(newObj, 50, 50);
#if LVGL_VERSION_MAJOR >= 9
    lv_arc_set_bg_angles(newObj, 90, 270);
#else
    lv_arc_set_angles(newObj, 90, 270);
#endif
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
