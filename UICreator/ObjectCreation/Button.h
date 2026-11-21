#pragma once
#include <lvgl/lvgl.h>

class Button
{
public:
    static lv_obj_t* Create(lv_obj_t* parent,
                            int x,
                            int y);
};
