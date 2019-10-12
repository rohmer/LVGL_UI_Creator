#include "ToolTray.h"
#include "PropertyWindow.h"
#include "ToolBar.h"
#undef GetObject
ToolTray::ToolTray(lv_obj_t *parent, 
	TreeView *objectTree, 
	lv_obj_t* drawSurface, 
	PropertyWindow *propWin,
	ToolBar *toolBar)
{
	objTree = objectTree;
	minWin = new MinimizableWindow("Tool Tray",
		0,
		0,
		lv_obj_get_width_fit(parent) / 4,
		lv_obj_get_height_fit(parent),
		nullptr,
		false,
		lv_obj_get_width_fit(parent)/2-40,
		10);
	this->toolBar = toolBar;
	toolWin = minWin->GetWindowObj();
	this->drawSurface = drawSurface;
	this->propertyWindow = propWin;
	lv_win_set_title(toolWin, "Tool Tray");	
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
		sCB *callbackStruct = new sCB();
		callbackStruct->toolTray = this;
		callbackStruct->typeID = i;
		lv_obj_set_user_data(button, (lv_obj_user_data_t)callbackStruct);
		lv_obj_set_event_cb(button, create_obj_cb);
		widgetButtons.push_back(button);
	}
}



#pragma endregion
#pragma endregion 

#pragma region Callback
void ToolTray::create_obj_cb(lv_obj_t * obj, lv_event_t ev)
{
	if (ev == LV_EVENT_CLICKED)
	{
		sCB* callbackStruct = (sCB*)lv_obj_get_user_data(obj);
		int objID = callbackStruct->typeID;
		ToolTray *tt = callbackStruct->toolTray;
		TreeView *tv = tt->objTree;		
		TreeNode *node = tv->GetSelectedNode();		
		lv_obj_t* parent, *newObj;
		if (node != nullptr)
		{
			parent = tv->GetSelectedNode()->GetLVObject();
			if (parent == nullptr)
				parent = tt->drawSurface;
		} else
		{
			parent = tt->drawSurface;
		}
		int x = 0, y=0;
		if (tt->lastWidget != nullptr)
		{
			x = lv_obj_get_x(tt->lastWidget) + 10;
			y = lv_obj_get_y(tt->lastWidget) + 10;			
		} 		
		newObj = nullptr;
		int parID = -1;
		if (tt->objTree->GetSelectedNode() != nullptr)
			parID = tt->objTree->GetSelectedNode()->GetID();
		else
			parID = 0;
		ObjectUserData *userData=new ObjectUserData();
		userData->parentID = parID;
		switch(objID)
		{
		case 0:
			newObj=Arc::Create(parent, x, y);
			tt->objTree->AddNode("Arc", newObj, parID, false);
			break;
		case 1:
			newObj = Bar::Create(parent, x, y);
			tt->objTree->AddNode("Bar", newObj, parID, false);
            break;
        case 2:
            newObj = Button::Create(parent, x, y);
            tt->objTree->AddNode("Button", newObj, parID, false);
            break;
        case 3:
            newObj = ButtonMatrix::Create(parent, x, y);
            tt->objTree->AddNode("Button Matrix", newObj, parID, false);
            break;
        case 4:
            newObj = Calendar::Create(parent, x, y);
            tt->objTree->AddNode("Calendar", newObj, parID, false);
            break;
            
		}
        userData->objectJson = Serialization::ObjectSerializer::SerializeObject(newObj);

		if (newObj == nullptr)
			return;
		tt->lastWidget = newObj;
        lv_obj_set_drag(newObj, true);	
		lv_obj_set_style(newObj, &lv_style_plain);
		lv_obj_set_protect(newObj, LV_PROTECT_PRESS_LOST);
		lv_obj_set_top(newObj, true);
		userData->objectID = tt->currentID++;
		userData->toolTray = tt;
		
		lv_obj_set_user_data(newObj, (lv_obj_user_data_t)userData);
		lv_obj_set_event_cb(newObj, updateProperties);
		tt->propertyWindow->SetSelectedObject(newObj);
		
	}
	
}

void ToolTray::updateProperties(lv_obj_t *obj, lv_event_t ev)
{
    if(ev==LV_EVENT_DRAG_END)
    {
        ObjectUserData* objectData = (ObjectUserData*)lv_obj_get_user_data(obj);
        objectData->toolTray->propertyWindow->SetSelectedObject(obj);
    }
	if (ev != LV_EVENT_CLICKED)
		return;
	
	ObjectUserData *objectData = (ObjectUserData *)lv_obj_get_user_data(obj);
	objectData->toolTray->toolBar->SetSelectedObject();
}
#pragma endregion
