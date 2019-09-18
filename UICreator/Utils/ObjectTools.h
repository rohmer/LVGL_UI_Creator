#pragma once

#include <vector>
#include <lvgl/lvgl.h>

class ObjectTools
{
public:
	static std::vector<lv_obj_t*> GetChildren(lv_obj_t* object);
};