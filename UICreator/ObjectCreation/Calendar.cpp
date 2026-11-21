#include "Calendar.h"
#include <ctime>

lv_obj_t* Calendar::Create(lv_obj_t* parent, int x, int y)
{
    // Creation should never be called with a null parent
    if (parent == nullptr)
        return nullptr;
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t* newObj = lv_calendar_create(parent);
    lv_calendar_add_header_arrow(newObj);
#else
    lv_obj_t* newObj = lv_calendar_create(parent, nullptr);
#endif
    lv_obj_set_size(newObj, 250, 200);
#if LVGL_VERSION_MAJOR >= 9
    const auto timestamp = std::time(nullptr);
    std::tm current{};
#ifdef _WIN32
    const bool valid = localtime_s(&current, &timestamp) == 0;
#else
    const bool valid = localtime_r(&timestamp, &current) != nullptr;
#endif
    if (const auto* today = valid ? &current : nullptr)
    {
        lv_calendar_set_today_date(newObj, today->tm_year + 1900, today->tm_mon + 1, today->tm_mday);
        lv_calendar_set_month_shown(newObj, today->tm_year + 1900, today->tm_mon + 1);
    }
#endif
    lv_obj_set_pos(newObj, x, y);
    return newObj;
}
