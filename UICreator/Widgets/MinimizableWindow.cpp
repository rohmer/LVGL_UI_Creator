#include "MinimizableWindow.h"

MinimizableWindow::MinimizableWindow(std::string title, 
	int x, 
	int y, 
	int width, 
	int height,
	lv_obj_t *parent,
	bool hasCloseBtn, 
	int iconX, 
	int iconY, 
	int iconWidth, 
	int iconHeight)
{
	if (parent == nullptr)
		parent = lv_scr_act();
	windowObject = lv_win_create(parent, nullptr);
	lv_obj_set_size(windowObject, width, height);
	lv_obj_set_pos(windowObject, x, y);
	lv_win_set_title(windowObject, title.c_str());
	if(hasCloseBtn)
	{
		closeBtn = lv_win_add_btn(windowObject, LV_SYMBOL_CLOSE);
		lv_obj_set_user_data(closeBtn, (lv_obj_user_data_t)this);
		lv_obj_set_event_cb(closeBtn, closeCB);
	}
	minimizeButton = lv_win_add_btn(windowObject, LV_SYMBOL_DOWN);
	lv_obj_set_user_data(minimizeButton, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(minimizeButton, minimizeCB);

	iconObject = lv_obj_create(parent, nullptr);
	lv_obj_set_style(iconObject, &lv_style_plain);
	lv_obj_set_size(iconObject, iconWidth, iconHeight);
	lv_obj_set_pos(iconObject, iconX, iconY);
	int titleBarSize = iconHeight / 6;
	lv_obj_t *iconTB = lv_obj_create(iconObject, nullptr);
	lv_obj_set_style(iconTB, &lv_style_plain_color);
	lv_obj_set_size(iconTB, iconWidth, titleBarSize);
	lv_obj_set_user_data(iconObject, (lv_obj_user_data_t)this);
	lv_obj_set_user_data(iconTB, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(iconTB, maximizeCB);
	lv_obj_set_event_cb(iconObject, maximizeCB);
	lv_obj_t *minLabel = lv_label_create(iconObject, nullptr);
	lv_obj_align(minLabel, iconObject, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_width(minLabel, iconWidth);
	lv_label_set_long_mode(minLabel, LV_LABEL_LONG_SROLL);
	lv_label_set_text(minLabel, title.c_str());
	lv_obj_set_hidden(iconObject, true);
	lv_obj_set_top(iconObject, true);
}

lv_obj_t *MinimizableWindow::GetWindowObj()
{
	return windowObject;
}

void MinimizableWindow::closeCB(lv_obj_t * obj, lv_event_t ev)
{
	if(ev != LV_EVENT_CLICKED)
		return;
	MinimizableWindow *mw = (MinimizableWindow*)lv_obj_get_user_data(obj);
	lv_obj_del_async(mw->iconObject);
	lv_obj_del_async(mw->windowObject);
}

void MinimizableWindow::maximizeCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
	MinimizableWindow *mw = (MinimizableWindow*)lv_obj_get_user_data(obj);
	lv_obj_set_hidden(mw->windowObject, false);
	lv_obj_set_hidden(mw->iconObject, true);
}

void MinimizableWindow::minimizeCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
	MinimizableWindow *mw = (MinimizableWindow*)lv_obj_get_user_data(obj);
	lv_obj_set_hidden(mw->windowObject, true);
	lv_obj_set_hidden(mw->iconObject, false);
	lv_obj_set_top(mw->iconObject, true);
}