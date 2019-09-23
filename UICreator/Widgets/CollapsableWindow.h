#pragma once
#include <string>
#include <vector>
#include "lvgl.h"
#include "../Utils/ObjectTools.h"

class CollapsableWindowManager;

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
	CollapsableWindow(lv_obj_t *parent,
		std::string name,
		bool isCollapsed,
		int width,
		int height);
	int GetCurrentHeight();
	lv_obj_t* GetWindow();

	void DeleteObjects();
	
	void AddObjectToWindow(lv_obj_t *obj);
	lv_obj_t* GetCollapseButton();
	const std::string GetName();
	void ToggleCollapsed();
	void SetPos(int x, int y);
	void SetVerticalPos(int y);
	void DeleteChildren();
	void UpdateHeight(int height);
	void UpdateWidth(int width);
private:
	lv_obj_t *window, *collapseBtnLbl, *collapseBtn, *winTitle;
	std::string name;
	bool isCollapsed;
	int x, y, width, height;
	struct cbStruct
	{
		CollapsableWindow *cw;
		CollapsableWindowManager *cwm;
	};
	void createObjects(lv_obj_t* parent);
	static void collapseClicked(lv_obj_t* obj, lv_event_t event);
	std::vector<lv_obj_t*> objects;
};
