#pragma once
#include <stdint.h>

// The prediction engine uses no LVGL implementation. These declarations allow
// its real public keyboard header to compile in this allocator-focused test.
typedef struct lv_obj_t lv_obj_t;
typedef struct { uint8_t blue, green, red; } lv_color_t;
