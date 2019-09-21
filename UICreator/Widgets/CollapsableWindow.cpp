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

CollapsableWindow::CollapsableWindow(lv_obj_t *parent,
	std::string name,
	bool isCollapsed,
	int width,
	int height) :
	name(name),
	isCollapsed(isCollapsed),
	x(0),
	y(0),
	width(width),
	height(height)
{
	createObjects(parent);
}

const std::string CollapsableWindow::GetName()
{
	return name;
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
	winTitle = lv_label_create(parent, nullptr);
	lv_label_set_text(winTitle, name.c_str());
	lv_obj_set_pos(winTitle, x + 25, 7);
	lv_obj_set_pos(collapseBtn, x+5, y+7);
	if(isCollapsed)
	{
		lv_label_set_text(collapseBtnLbl, LV_SYMBOL_RIGHT);
	} else
	{
		lv_label_set_text(collapseBtnLbl, LV_SYMBOL_DOWN);
	}
	lv_cont_set_layout(window, LV_LAYOUT_OFF);
	lv_cont_set_fit(window, LV_FIT_TIGHT);
	lv_obj_set_pos(window, x, y+25);

	lv_obj_set_style(window, &lv_style_transp);
	cbStruct *userData=new cbStruct();
	userData->cw = this;
	lv_obj_set_user_data(collapseBtn, userData);
	lv_obj_set_event_cb(collapseBtn, collapseClicked);
}

void CollapsableWindow::SetPos(int x, int y)
{
	this->x = x;
	this->y = y;
	lv_obj_set_pos(winTitle,x + 25, 7+y);
	lv_obj_set_pos(collapseBtn, x + 5, 7+y);
	lv_obj_set_pos(window, x, y + 25);
}

void CollapsableWindow::SetVerticalPos(int y)
{
	this->y = y;
	lv_obj_set_pos(winTitle, this->x + 25, 7 + y);
	lv_obj_set_pos(collapseBtn, this->x + 5, 7 + y);
	lv_obj_set_pos(window, this->x, y + 25);

}

void CollapsableWindow::ToggleCollapsed()
{
	if (isCollapsed)
		isCollapsed = false;
	else
		isCollapsed = true;
	if(isCollapsed)
	{
		lv_label_set_text(collapseBtnLbl, LV_SYMBOL_RIGHT);
		lv_cont_set_fit(window, LV_FIT_NONE);
		lv_obj_set_size(window, width, 0);
	} else
	{
		lv_label_set_text(collapseBtnLbl, LV_SYMBOL_DOWN);
		lv_obj_set_size(window, width, height);
		lv_cont_set_fit(window, LV_FIT_TIGHT);
	}
}

void CollapsableWindow::collapseClicked(lv_obj_t* obj, lv_event_t event)
{
	if (event != LV_EVENT_CLICKED)
		return;
	cbStruct *cb= (cbStruct*)lv_obj_get_user_data(obj);
	CollapsableWindow *cw = cb->cw;
	if(cw->isCollapsed)
	{
		lv_label_set_text(cw->collapseBtnLbl, LV_SYMBOL_DOWN);
		cw->isCollapsed = false;
		lv_obj_set_size(cw->window, cw->width, cw->height);
		lv_cont_set_layout(cw->window, LV_FIT_TIGHT);
	}
	else
	{
		lv_label_set_text(cw->collapseBtnLbl, LV_SYMBOL_RIGHT);
		cw->isCollapsed = true;
		// WE need to hide everything in the window
		lv_cont_set_layout(cw->window, LV_FIT_NONE);
		lv_obj_set_size(cw->window, cw->width, 0);
	}
}

int CollapsableWindow::GetCurrentHeight()
{
	std::vector<lv_obj_t*> children = ObjectTools::GetChildren(window);	
	return (lv_obj_get_height(window));	
}

lv_obj_t* CollapsableWindow::GetWindow()
{
	return window;
}

void CollapsableWindow::AddObjectToWindow(lv_obj_t* obj)
{
	lv_obj_set_parent(obj, window);
	lv_obj_set_y(obj,lv_obj_get_y(obj) + 10);
}

lv_obj_t* CollapsableWindow::GetCollapseButton()
{
	return collapseBtn;
}
