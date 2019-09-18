#include "ToolTray.h"

ToolTray::ToolTray(lv_obj_t *parent)
{
	toolWin= lv_win_create(parent, nullptr);
	lv_win_set_title(toolWin, "Tool Tray");
	lv_obj_set_size(toolWin, lv_obj_get_width_fit(parent) / 4, lv_obj_get_height_fit(parent));
	lv_obj_align(toolWin, nullptr, LV_ALIGN_IN_LEFT_MID, 0, 0);
	initializeToolMatrix();
}

#pragma region Tool Matrix
void ToolTray::initializeToolMatrix()
{
	lv_obj_t *tabView = lv_tabview_create(toolWin, nullptr);
	lv_obj_t *widgetTab = lv_tabview_add_tab(tabView, "Widgets");
	lv_obj_t *dialogTab = lv_tabview_add_tab(tabView, "Dialogs");
	lv_obj_t *customTab = lv_tabview_add_tab(tabView, "Custom");
	initializeWidgetButtons(widgetTab);
}

#pragma region Widget Buttons
void ToolTray::initializeWidgetButtons(lv_obj_t* parent)
{
	int width = lv_obj_get_width_fit(parent)/3-5;
	int height = 61;

	std::string bNames[30] = { "Arc", "Bar", "Button", "Button\nmatrix","Calendar","Canvas","Check\n  box",
		"Chart", "Container", "Drop down\n       list","Gauge","Image","Image\nbutton","Keyboard","Label","LED",
		"Line"," Line\nmeter","Message\n     box","Page","Preloader","Roler","Slider","Spinbox","Switch",
		"Table","Tabview","Text\narea"," Tile\nview","Window"};
	int xCtr = 0;
	int yCtr = 0;

	for(int i=0; i<30; i++)
	{
		lv_obj_t *button = lv_btn_create(parent, nullptr);
		lv_obj_set_user_data(button, (lv_obj_user_data_t)bNames[i].c_str());
		int x = (width * xCtr) + (5 * xCtr)+4;
		int y = (height * yCtr) + (5 * yCtr) + 5;
		lv_obj_set_size(button, width, height);
		lv_obj_set_pos(button, x,y);
		
		
		lv_obj_t *bLabel = lv_label_create(button, nullptr);
		lv_label_set_text(bLabel, bNames[i].c_str());
		xCtr++;
		if(xCtr>2)
		{
			xCtr = 0;
			yCtr++;
		}
		lv_obj_set_user_data(button, (lv_obj_user_data_t)i);
		lv_obj_set_event_cb(button, create_obj_cb);
		widgetButtons.push_back(button);
	}
}



#pragma endregion
#pragma endregion 

#pragma region Callback
void ToolTray::create_obj_cb(lv_obj_t * obj, lv_event_t ev)
{
	int objID = (int)lv_obj_get_user_data(obj);
}

#pragma endregion