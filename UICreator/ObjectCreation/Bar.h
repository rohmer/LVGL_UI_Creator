#pragma once
#include <string>
#include <lvgl/lvgl.h>

#include "../Widgets/TreeView.h"
#include "../Windows/UIObjectData.h"

class Bar
{
public:
    static lv_obj_t* Create(lv_obj_t* parent,
                            int x,
                            int y);
};
