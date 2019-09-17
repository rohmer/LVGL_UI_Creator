#include "CollapsableWindow.h"

CollapsableWindow::CollapsableWindow(lv_obj_t *parent,
	std::string name, 
	bool isCollapsed, 
	int x, 
	int y, 
	int width, 
	int height) :
	name(name),
	isCollapsed(isCollapsed),
	x(x),
	y(y),
	width(width),
	height(height)
{
	createObjects(parent);
}

void CollapsableWindow::createObjects(lv_obj_t *parent)
{
	if (parent == nullptr)
	{
		window = lv_cont_create(lv_scr_act(), nullptr);
	}
	else
	{
		window = lv_cont_create(parent, nullptr);
	}
	lv_obj_set_size(window, width, height);
	
	collapseBtn=lv_btn_create(parent, nullptr);
	collapseBtnLbl = lv_label_create(collapseBtn, nullptr);
	lv_obj_set_size(collapseBtn, 15, 15);
	lv_obj_t *winTitle = lv_label_create(parent, nullptr);
	lv_label_set_text(winTitle, name.c_str());
	lv_obj_set_pos(winTitle, x + 25, 7);
	lv_obj_set_pos(collapseBtn, x+5, y+7);
	if(isCollapsed)
	{
		lv_label_set_text(collapseBtnLbl, LV_SYMBOL_RIGHT);
		lv_obj_set_size(window, width, lv_obj_get_y(collapseBtn));
	} else
	{
		lv_label_set_text(collapseBtnLbl, LV_SYMBOL_DOWN);
		lv_obj_set_size(window, width, height);
	}
	lv_obj_set_pos(window, x, y + lv_obj_get_height(collapseBtn));

	lv_obj_set_style(window, &lv_style_transp);
	lv_obj_set_user_data(collapseBtn, this);
	lv_obj_set_event_cb(collapseBtn, collapseClicked);
}

void CollapsableWindow::collapseClicked(lv_obj_t* obj, lv_event_t event)
{
	if (event != LV_EVENT_CLICKED)
		return;
	CollapsableWindow *cw = (CollapsableWindow*)lv_obj_get_user_data(obj);
	if(cw->isCollapsed)
	{
		lv_label_set_text(cw->collapseBtnLbl, LV_SYMBOL_DOWN);
		cw->isCollapsed = false;
		lv_obj_set_size(cw->window, cw->width, cw->height);

	}
	else
	{
		lv_label_set_text(cw->collapseBtnLbl, LV_SYMBOL_RIGHT);
		cw->isCollapsed = true;
		// WE need to hide everything in the window
		lv_obj_set_size(cw->window, cw->width, 0);
		
	}
}

int CollapsableWindow::GetCurrentHeight()
{
	return lv_obj_get_y(window);
}

lv_obj_t* CollapsableWindow::GetWindow()
{
	return window;
}

void CollapsableWindow::AddObjectToWindow(lv_obj_t* obj)
{
	lv_obj_set_parent(obj, window);
}

lv_obj_t* CollapsableWindow::GetCollapseButton()
{
	return collapseBtn;
}
