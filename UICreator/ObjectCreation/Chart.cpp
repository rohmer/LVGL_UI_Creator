#include "Chart.h"

lv_obj_t* Chart::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent
    if (parent == nullptr)
        return nullptr;
    lv_obj_t* newObj = lv_chart_create(parent, nullptr);
    lv_obj_set_size(newObj, 80, 80);
    lv_obj_set_parent(newObj, parent);
    lv_obj_set_pos(newObj, x, y);
    lv_chart_set_type(newObj, LV_CHART_TYPE_POINT | LV_CHART_TYPE_LINE); /*Show lines and points too*/
    lv_chart_set_series_opa(newObj, LV_OPA_70); /*Opacity of the data series*/
    lv_chart_set_series_width(newObj, 4); /*Line width and point radious*/

    lv_chart_set_range(newObj, 0, 100);

    /*Add two data series*/
    lv_chart_series_t* ser1 = lv_chart_add_series(newObj, lv_color_make(255, 0, 0));
    lv_chart_series_t* ser2 = lv_chart_add_series(newObj, lv_color_make(0, 255, 0));

    /*Set the next points on 'dl1'*/
    lv_chart_set_next(newObj, ser1, 10);
    lv_chart_set_next(newObj, ser1, 10);
    lv_chart_set_next(newObj, ser1, 10);
    lv_chart_set_next(newObj, ser1, 10);
    lv_chart_set_next(newObj, ser1, 10);
    lv_chart_set_next(newObj, ser1, 10);
    lv_chart_set_next(newObj, ser1, 10);
    lv_chart_set_next(newObj, ser1, 30);
    lv_chart_set_next(newObj, ser1, 70);
    lv_chart_set_next(newObj, ser1, 90);

    /*Directly set points on 'dl2'*/
    ser2->points[0] = 90;
    ser2->points[1] = 70;
    ser2->points[2] = 65;
    ser2->points[3] = 65;
    ser2->points[4] = 65;
    ser2->points[5] = 65;
    ser2->points[6] = 65;
    ser2->points[7] = 65;
    ser2->points[8] = 65;
    ser2->points[9] = 65;

    lv_chart_refresh(newObj); /*Required after direct set*/
    return newObj;
}
