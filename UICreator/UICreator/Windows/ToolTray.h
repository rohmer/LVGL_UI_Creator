#pragma once
#include <string>
#include <vector>
#include "lvgl/lvgl.h"

class ToolTray
{
public:
	ToolTray(lv_obj_t *parent);
	
private:
	lv_obj_t *toolWin;
	void initializeToolMatrix();
	void initializeWidgetButtons(lv_obj_t* parent);

	static void create_obj_cb(lv_obj_t * obj, lv_event_t ev);
	
	std::vector<lv_obj_t*> widgetButtons;
};