#include "Chart.h"

lv_obj_t* Chart::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent
    if (parent == nullptr)
        return nullptr;
    static const int32_t firstSeries[] = {10, 10, 10, 10, 10, 10, 10, 30, 70, 90};
    static const int32_t secondSeries[] = {90, 70, 65, 65, 65, 65, 65, 65, 65, 65};
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t* newObj = lv_chart_create(parent);
    lv_chart_set_type(newObj, LV_CHART_TYPE_LINE);
    lv_obj_set_style_line_opa(newObj, LV_OPA_70, LV_PART_ITEMS);
    lv_obj_set_style_line_width(newObj, 4, LV_PART_ITEMS);
    lv_obj_set_style_size(newObj, 8, 8, LV_PART_INDICATOR);
    lv_chart_set_axis_range(newObj, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(newObj, 10);

    lv_chart_series_t* first = lv_chart_add_series(newObj, lv_color_make(255, 0, 0), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_series_t* second = lv_chart_add_series(newObj, lv_color_make(0, 255, 0), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_series_values(newObj, first, firstSeries, 10);
    lv_chart_set_series_values(newObj, second, secondSeries, 10);
#else
    lv_obj_t* newObj = lv_chart_create(parent, nullptr);
    lv_chart_set_type(newObj, LV_CHART_TYPE_POINT | LV_CHART_TYPE_LINE);
    lv_chart_set_series_opa(newObj, LV_OPA_70);
    lv_chart_set_series_width(newObj, 4);
    lv_chart_set_range(newObj, 0, 100);

    lv_chart_series_t* first = lv_chart_add_series(newObj, lv_color_make(255, 0, 0));
    lv_chart_series_t* second = lv_chart_add_series(newObj, lv_color_make(0, 255, 0));
    for (int32_t value : firstSeries)
        lv_chart_set_next(newObj, first, value);
    for (int32_t value : secondSeries)
        lv_chart_set_next(newObj, second, value);
#endif
    lv_obj_set_size(newObj, 80, 80);
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
