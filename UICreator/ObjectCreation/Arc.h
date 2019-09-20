#pragma once
#include <string>
#include <lvgl/lvgl.h>

#include "../Widgets/TreeView.h"
#include "../Serialization/LVArc.h"
#include "../Windows/UIObjectData.h"

class Arc
{
public:
	static void Create(lv_obj_t *parent, 
		TreeView *objectTree,
		TreeNode *parentTreeNode);
};