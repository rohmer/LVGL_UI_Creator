#pragma once
#include <string>
#include <vector>
#include "lvgl.h"

class CollapsableWindow
{
public:
	CollapsableWindow(lv_obj_t *parent, 
		std::string name, 
		bool isCollapsed, 
		int x, 
		int y, 
		int width, 
		int height);

	int GetCurrentHeight();
	lv_obj_t* GetWindow();

	void AddObjectToWindow(lv_obj_t *obj);
	lv_obj_t* GetCollapseButton();
	
private:
	lv_obj_t *window, *collapseBtnLbl, *collapseBtn;
	std::string name;
	bool isCollapsed;
	int x, y, width, height;

	void createObjects(lv_obj_t* parent);
	static void collapseClicked(lv_obj_t* obj, lv_event_t event);	
};