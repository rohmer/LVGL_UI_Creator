#pragma once
#include "Metrics.h"
#include <lvgl/lvgl.h>
#include <functional>
namespace PerformanceWindow {
struct Context {
    std::function<Performance::Snapshot()> snapshot;
    std::function<void(bool)> enable;
    std::function<Performance::Distribution(const std::string&,unsigned)> benchmark;
    lv_group_t* keyboardGroup=nullptr;
};
void Open(Context context);
void Close();
lv_obj_t* ActiveWindow();
}
