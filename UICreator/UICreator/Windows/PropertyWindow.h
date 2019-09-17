#pragma once
#include <string>
#include <vector>
#include "lvgl/lvgl.h"
#include "SimWindow.h"
#include "../Widgets/CollapsableWindow.h"
#include "Themes.h"
class PropertyWindow
{
public:
	PropertyWindow(SimWindow *simWindow, int scrWidth, int scrHeight);
	
private:
	SimWindow *simWindow;
	lv_obj_t* propertyWin;
	int screenX, screenY;
	bool globalExpanded;
	CollapsableWindow *globalProps;
	static lv_theme_t *activeTheme;
	static std::vector<lv_theme_t *> themes;
	
	static void initializeThemes(uint16_t hue);
	void createPropertyWin();
	void createGlobalProps();

	static void theme_select_event_handler(lv_obj_t * roller, lv_event_t event);
	static void hue_select_event_cb(lv_obj_t * roller, lv_event_t event);
};
