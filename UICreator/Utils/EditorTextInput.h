#pragma once
#include <lvgl/lvgl.h>

#if LVGL_VERSION_MAJOR >= 9
bool HandleEditorTextShortcut(lv_group_t* group, int key);
bool ReplaceEditorTextSelection(lv_group_t* group, const char* text);
bool DeleteEditorTextSelection(lv_group_t* group);
void ClearEditorTextSelection(lv_group_t* group);
#endif
