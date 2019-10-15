#include <lvgl/lvgl.h>

#include "../Widgets/TreeView.h"
#include "../Windows/UIObjectData.h"

class Calendar
{
public:
    static lv_obj_t* Create(lv_obj_t* parent,
                            int x,
                            int y);
};
