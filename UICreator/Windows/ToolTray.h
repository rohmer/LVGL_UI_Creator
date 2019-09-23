#pragma once
#include <string>
#include <vector>
#include "lvgl/lvgl.h"
#include "../Widgets/TreeView.h"
#include "../Widgets/MinimizableWindow.h"

#pragma region Object Creation Includes
#include "../ObjectCreation/Arc.h"
#pragma endregion

class PropertyWindow;

class ToolTray
{
public:
	ToolTray(lv_obj_t *parent, TreeView *objectTree, lv_obj_t *drawSurface, PropertyWindow *propWin);
	
private:
	lv_obj_t *toolWin;
	MinimizableWindow *minWin;
	TreeView *objTree;
	lv_obj_t *drawSurface;
	PropertyWindow *propertyWindow;
	void initializeToolMatrix();
	void initializeWidgetButtons(lv_obj_t* parent);
	
	std::vector<lv_obj_t*> widgetButtons;
	lv_obj_t *lastWidget = nullptr;
	struct sCB
	{
		ToolTray *toolTray;
		int typeID;
	};
	
	struct sObjStruct
	{
		ToolTray *toolTray;
		json objectJson;
	};

#pragma region Callbacks
	static void create_obj_cb(lv_obj_t *obj, lv_event_t ev);
	static void updateProperties(lv_obj_t *obj, lv_event_t ev);
#pragma endregion

};