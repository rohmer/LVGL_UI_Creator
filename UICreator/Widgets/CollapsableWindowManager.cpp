#include "CollapsableWindowManager.h"

CollapsableWindowManager::CollapsableWindowManager(lv_obj_t* parent, 
	int x,
	int y,
	int verticalPadding,
	bool drawSeperators) :
	parent(parent),
	x(x),
	y(y),
	verticalPadding(verticalPadding),
	seperators(drawSeperators)
{
	initialY = y;
}

void CollapsableWindowManager::AddWindow(CollapsableWindow *window)
{
	window->SetPos(x,y);
	y += window->GetCurrentHeight()+verticalPadding;
	if(seperators)
	{
		y += 3;
		lv_obj_t *s = lv_obj_create(this->parent, nullptr);
		lv_obj_set_width(s, lv_obj_get_width(this->parent) - 8);
		lv_obj_set_height(s, 5);
		lv_obj_set_x(s, 4);
		lv_obj_set_y(s, y);
		lv_obj_set_hidden(s, false);
		lv_obj_set_style(s, &lv_style_pretty_color);
		lv_obj_move_foreground(s);
		seps.push_back(s);
		y += 3;
	}
	windows.push_back(window);
	cbStruct *cb = new cbStruct();
	cb->cw = window;
	cb->cwm = this;
	lv_obj_set_user_data(window->GetCollapseButton(), cb);
	lv_obj_set_event_cb(window->GetCollapseButton(), expandCallback);
}

void CollapsableWindowManager::expandCallback(lv_obj_t* obj, lv_event_t event)
{
	if (event != LV_EVENT_CLICKED)
		return;
	cbStruct *cb = (cbStruct *)lv_obj_get_user_data(obj);
	cb->cw->ToggleCollapsed();

	cb->cwm->y = cb->cwm->initialY;
	int ctr = 0;
	// We now need to reposition all our windows
	for(std::vector<CollapsableWindow*>::iterator it=cb->cwm->windows.begin(); 
		it!=cb->cwm->windows.end();
		++it)
	{
		(*it)->SetVerticalPos(cb->cwm->y);
		if(cb->cwm->seperators)
		{
			lv_obj_set_y(cb->cwm->seps[ctr], cb->cwm->y+ (*it)->GetCurrentHeight()+cb->cwm->verticalPadding+3);
			cb->cwm->y += 6;
			ctr++;
		}
		cb->cwm->y += (*it)->GetCurrentHeight() + cb->cwm->verticalPadding;
	}
}
