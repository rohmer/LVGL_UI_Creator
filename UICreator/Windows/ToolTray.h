#pragma once
#include <string>
#include <vector>
#include "lvgl/lvgl.h"
#include "../Widgets/TreeView.h"

#pragma region Object Creation Includes
#include "../ObjectCreation/Arc.h"
#pragma endregion

class ToolTray
{
public:
	ToolTray(lv_obj_t *parent, TreeView *objectTree);
	
private:
	lv_obj_t *toolWin;
	TreeView *objTree;
	void initializeToolMatrix();
	void initializeWidgetButtons(lv_obj_t* parent);

	static void create_obj_cb(lv_obj_t * obj, lv_event_t ev);
	
	std::vector<lv_obj_t*> widgetButtons;

	struct sCB
	{
		ToolTray *toolTray;
		int typeID;
	};

};