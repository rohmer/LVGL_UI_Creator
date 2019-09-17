#include "CollapsableWindowManager.h"

CollapsableWindowManager::CollapsableWindowManager(lv_obj_t* parent, 
	int x,
	int y,
	int verticalPadding) :
	parent(parent),
	x(x),
	y(y),
	verticalPadding(verticalPadding)
{
	initialY = y;
}

void CollapsableWindowManager::AddWindow(CollapsableWindow *window)
{
	window->SetPos(x,y);
	y += window->GetCurrentHeight()+verticalPadding;
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
	// We now need to reposition all our windows
	for(std::vector<CollapsableWindow*>::iterator it=cb->cwm->windows.begin(); 
		it!=cb->cwm->windows.end();
		++it)
	{
		(*it)->SetVerticalPos(cb->cwm->y);
		cb->cwm->y += (*it)->GetCurrentHeight() + cb->cwm->verticalPadding;
	}
}
