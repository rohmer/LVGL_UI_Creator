#include "ObjectTools.h"

std::vector<lv_obj_t*> ObjectTools::GetChildren(lv_obj_t* object)
{
    std::vector<lv_obj_t*> children;
#if LVGL_VERSION_MAJOR >= 9
    // LVGL 6 enumerates the newest child first.
    for (auto remaining = lv_obj_get_child_count(object); remaining > 0; --remaining)
    {
        lv_obj_t* child = lv_obj_get_child(object, remaining - 1);
#else
    for (lv_obj_t* child = lv_obj_get_child(object, nullptr); child != nullptr;
         child = lv_obj_get_child(object, child))
    {
#endif
        children.push_back(child);
        std::vector<lv_obj_t*> descendants = GetChildren(child);
        children.insert(children.end(), descendants.begin(), descendants.end());
    }

    return children;
}
