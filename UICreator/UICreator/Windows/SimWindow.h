#pragma once

#include "lvgl/lvgl.h"
#include <sstream>
#include <string>
#include <vector>

#include "../Utils/ObjectTools.h"

class SimWindow
{
public:
	SimWindow(int iWidth, int iHeight);

private:
	int width, height;
	lv_obj_t *window, *settingsWindow, *drawSurface;

	struct cbStruct
	{
		SimWindow *sw;
		bool pos, sze;
		lv_obj_t *width, *height, *kb, *setBtn;
	};

	void resize(int newWidth, int newHeight, bool repos, bool resize);
	
	void createObjects();
	static void settings(lv_obj_t *obj, lv_event_t event);
	static void closeSettings(lv_obj_t *obj, lv_event_t event);
	static void widthClicked(lv_obj_t *obj, lv_event_t event);
	static void heightClicked(lv_obj_t *obj, lv_event_t event);
	static void submitClicked(lv_obj_t *obj, lv_event_t event);
};
