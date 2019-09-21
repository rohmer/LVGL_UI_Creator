#pragma once
#include <vector>

#include <lvgl/lvgl.h>
#include "CollapsableWindow.h"

class CollapsableWindowManager
{
public:
	CollapsableWindowManager(lv_obj_t* parent, int x=10, int y = 5, int verticalPadding=30, bool drawSeperators=true);
	void AddWindow(CollapsableWindow *window);
	
private:
	lv_obj_t* parent;
	int x, y, verticalPadding, initialY;
	std::vector<CollapsableWindow*> windows;
	bool seperators;
	std::vector<lv_obj_t*> seps;
	
	struct cbStruct
	{
		std::size_t hash;
		CollapsableWindow *cw;
		CollapsableWindowManager *cwm;
	};
	static void expandCallback(lv_obj_t *obj, lv_event_t event);
};