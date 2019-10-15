#pragma once
#include <string>
#include <lvgl/lvgl.h>

#include "../Windows/UIObjectData.h"

class Button
{
public:
    static lv_obj_t* Create(lv_obj_t* parent,
                            int x,
                            int y);
};
