#pragma once
#include <lvgl/lvgl.h>
#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>
namespace CommandPalette {
struct Entry {std::string label,detail,shortcut;std::function<void()> invoke;bool enabled=true;std::string action;};
void Open(const std::string& title,std::vector<Entry> entries,lv_group_t* keyboardGroup);
void Close();
bool HandleEvent(const SDL_Event& event);
lv_obj_t* ActiveWindow();
size_t ResultCount();
}
