#pragma once

#include <string>
#include "../../3rdParty/JSON/json.hpp"
#include <lvgl/lvgl.h>

#include "../Widgets/TreeView.h"

using json=nlohmann::json;
class ToolBar;

class PropertySelection
{
public:
	PropertySelection(ToolBar *toolBar);
	~PropertySelection();
	void Show(json j);
	
private:
	TreeView *treeView;
	ToolBar *toolBar;

	lv_obj_t *window, *closeButton, *okButton;

	void createObjects();

	void addNode(int parentID, json j);
	
	static void closeCB(lv_obj_t *obj, lv_event_t event);
	static void okCB(lv_obj_t *obj, lv_event_t event);
};
