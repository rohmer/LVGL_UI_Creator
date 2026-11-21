#include "Container.h"

lv_obj_t* Container::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent
    if (parent == nullptr)
        return nullptr;
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t* newObj = lv_obj_create(parent);
    lv_obj_set_size(newObj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(newObj, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(newObj, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
#else
    lv_obj_t* newObj = lv_cont_create(parent, nullptr);
    lv_obj_set_size(newObj, 80, 80);
    lv_cont_set_fit(newObj, LV_FIT_TIGHT);
    lv_cont_set_layout(newObj, LV_LAYOUT_PRETTY);
#endif
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
