#pragma once
#include <lvgl/lvgl.h>

#if LVGL_VERSION_MAJOR >= 9
bool IsEditorInputControl(const lv_obj_t* control);
bool IsEditorControlAvailable(const lv_obj_t* control);
lv_group_t* EditorKeyboardGroup();
void FocusEditorControl(lv_group_t* group, lv_obj_t* control);
void BindEditorControl(lv_group_t* group, lv_obj_t* control);
#endif
