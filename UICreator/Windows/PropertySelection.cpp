#include "PropertySelection.h"
#include "ToolBar.h"

PropertySelection::PropertySelection(ToolBar* toolBar) :
	toolBar(toolBar)
{
	window = lv_win_create(lv_scr_act(), nullptr);
	closeButton=lv_win_add_btn(window, LV_SYMBOL_CLOSE);
	lv_win_set_title(window, "Select a Property");	
	lv_obj_set_size(window, 300, 450);
	lv_obj_set_pos(window, lv_obj_get_width(lv_scr_act()) / 2 - 150,
		lv_obj_get_height(lv_scr_act()) / 2 - 225);
	lv_obj_set_user_data(closeButton, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(closeButton, closeCB);
	createObjects();
}

PropertySelection::~PropertySelection()
{
	if (treeView)
		delete(treeView);
	lv_obj_del_async(window);
}

void PropertySelection::createObjects()
{
	treeView = new TreeView(0, 0, 285, 300, "Properties", false, window);
	okButton = lv_btn_create(window, nullptr);
	lv_obj_set_size(okButton, 200, 50);
	lv_obj_t *lbl = lv_label_create(okButton, nullptr);
	lv_label_set_text(lbl, "Select Property and Close");
	lv_obj_set_pos(okButton, 50, 320);
		
	lv_obj_set_event_cb(okButton, okCB);
	lv_obj_set_hidden(window, true);
}

void PropertySelection::addNode(int parentID, json j)
{
	for (auto it = j.begin(); it != j.end(); ++it)
	{
		int id = treeView->AddNode(it.key(), nullptr, parentID, false);
		if (it->is_structured())
		{
			addNode(id, *it);
		}
	}
}

void PropertySelection::Show(json j)
{
	lv_obj_set_hidden(window, false);
	int rootNode=treeView->AddNode("", nullptr, 0, true);
	
	for (auto it = j.begin(); it != j.end(); ++it)
	{
		int id = treeView->AddNode(it.key(), nullptr, rootNode, false);
		if(it->is_structured())
		{			
			addNode(id, *it);
		}
	}
}

void PropertySelection::closeCB(lv_obj_t* obj, lv_event_t event)
{
	
}

void PropertySelection::okCB(lv_obj_t* obj, lv_event_t event)
{
	
}
