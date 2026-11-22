#pragma once
#include <lvgl/lvgl.h>

class UI;
namespace FigmaImportWindow {
void Open(UI* editor, lv_group_t* keyboardGroup);
void Close();
}
