#pragma once
#include <string>
#include <map>
#include <vector>
#include "lvgl/lvgl.h"
#include "SimWindow.h"
#include "../Widgets/CollapsableWindowManager.h"
#include "../Widgets/CollapsableWindow.h"
#include "../Widgets/MinimizableWindow.h"
#include "../Widgets/TreeView.h"
#include "UIObjectData.h"
#include "../JSON/json.hpp"
#include "../Serialization/LVObject.h"
#include "../Serialization/Style.h"

using json=nlohmann::json;

class PropertyWindow
{
public:
	PropertyWindow(SimWindow *simWindow, int screenWidth, int screenHeight);
	TreeView *GetObjectTree();
	void SetSelectedObject(lv_obj_t *object);
	void AddStyle(json styleJson);
	
private:
	MinimizableWindow *minWin;
	SimWindow *simWindow;
	lv_obj_t* propertyWin;
	int screenX, screenY;
	bool globalExpanded;
	CollapsableWindow *globalProps, *baseObjProps, *treeWin, *objProps;
	CollapsableWindowManager *cwm;
	TreeView *treeView;
	static lv_theme_t *activeTheme;
	static std::vector<lv_theme_t *> themes;

	lv_obj_t *selectedObject;

#pragma region Global Property Inputs
	// Position/Area
	lv_obj_t *taX, *taY, *taWidth, *taHeight;
	lv_obj_t *styleDD;
	lv_obj_t *hidden, *click, *top, *parentEvent, *opaScaleEnable, *opaScale;
	lv_obj_t *drag, *dragDir, *dragThrow, *dragParent;
#pragma endregion

	std::map<std::string, json> styles;
	
	static void initializeThemes(uint16_t hue);
	void createPropertyWin();
	void createGlobalProps();
	void createBaseObjProps();
	void createObjProps();
	void createTreeView();

	lv_obj_t *createNumericEntry(lv_obj_t *parent, const std::string labelTxt);
	void initStyles();
	
	static void objSelectCB(TreeNode *node);
	static void deleteCB(TreeNode *node);	
	static void theme_select_event_handler(lv_obj_t * roller, lv_event_t event);
	static void hue_select_event_cb(lv_obj_t * roller, lv_event_t event);
	static void numericEntryCB(lv_obj_t *obj, lv_event_t event);
	static void checkBoxCB(lv_obj_t *obj, lv_event_t event);
	
	static void createStyleCB(lv_obj_t *obj, lv_event_t event);
};
