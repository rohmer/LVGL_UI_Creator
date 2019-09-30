#pragma once
#include <string>
#include <lvgl/lvgl.h>

#include "../Widgets/TreeView.h"
#include "../Serialization/LVArc.h"
#include "../Windows/UIObjectData.h"

class Arc
{
public:
	static lv_obj_t* Create(lv_obj_t *parent, 		
		int x,
		int y);
};