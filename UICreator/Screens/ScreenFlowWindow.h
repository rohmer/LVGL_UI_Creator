#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <lvgl/lvgl.h>
#include <functional>
namespace ScreenFlowWindow {
struct Context{std::function<nlohmann::json()> document;std::function<void(nlohmann::json)> apply;std::function<void(size_t)> selectScreen;std::function<void()> run;lv_group_t* keyboardGroup=nullptr;};
void Open(Context context);void Close();void Refresh();lv_obj_t* ActiveWindow();
}
