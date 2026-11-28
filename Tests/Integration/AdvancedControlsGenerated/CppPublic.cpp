#include "ui.h"

extern "C" bool advanced_controls_cpp_api(lv_obj_t* radio)
{
    return ui_advanced_get_selected(radio) == 1 && ui_advanced_part(radio, "Option1") != nullptr;
}
