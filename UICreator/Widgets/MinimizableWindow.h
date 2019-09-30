#pragma once

#include <string>

#include <lvgl.h>

class MinimizableWindow
{
public:
	MinimizableWindow(std::string title, 
		int x, 
		int y, 
		int width, 
		int height,
		lv_obj_t *parent=nullptr,
		bool hasCloseBtn=true,
		int iconX=10,
		int iconY=10,
		int iconWidth=48,
		int iconHeight=48);
	lv_obj_t *GetWindowObj();
	~MinimizableWindow();
	
private:
	bool isMinimized = false;
	int x, y, width, height;
	int iconX, iconY, iconWidth, iconHeight;

	lv_obj_t *windowObject, *iconObject, *closeBtn, *minimizeButton;

	static void closeCB(lv_obj_t * obj, lv_event_t ev);
	static void minimizeCB(lv_obj_t * obj, lv_event_t ev);
	static void maximizeCB(lv_obj_t *obj, lv_event_t ev);
};