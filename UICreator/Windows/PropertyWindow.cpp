#include "PropertyWindow.h"
#include "UI.h"

lv_theme_t *PropertyWindow::activeTheme;
std::vector<lv_theme_t*> PropertyWindow::themes;

PropertyWindow::PropertyWindow(SimWindow* simWindow, int screenWidth, int screenHeight) :
	simWindow(simWindow),
	screenX(screenWidth),
	screenY(screenHeight)
{
	initializeThemes(0);
	createPropertyWin();
}

TreeView* PropertyWindow::GetObjectTree()
{
	return treeView;
}

void PropertyWindow::createPropertyWin()
{
	propertyWin = lv_win_create(lv_scr_act(), nullptr);
	lv_obj_t* curScr = lv_scr_act();
	lv_win_set_title(propertyWin, "Properties");
	lv_obj_set_size(propertyWin, 400, screenY);
	lv_obj_set_x(propertyWin, screenX-400);
	lv_obj_set_drag(propertyWin, true);
	lv_obj_set_drag_dir(propertyWin, LV_DRAG_DIR_ALL);
	lv_obj_set_protect(propertyWin, LV_PROTECT_NONE);
	cwm = new CollapsableWindowManager(propertyWin);
	createGlobalProps();
	createBaseObjProps();

	// TreeView is the last property
	createTreeView();
}

void PropertyWindow::createBaseObjProps()
{
	baseObjProps = new CollapsableWindow(propertyWin, "Base Object", false, 10,0,0, 0);
	cwm->AddWindow(baseObjProps);
}

void PropertyWindow::createGlobalProps()
{
	globalProps = new CollapsableWindow(propertyWin, "Global", false, 10, 0, 400, 200);
	lv_obj_t* win = globalProps->GetWindow();
	lv_obj_t * th_roller = lv_roller_create(win, NULL);
	lv_roller_set_options(th_roller, th_options, true);
	lv_obj_set_event_cb(th_roller, theme_select_event_handler);

	lv_obj_t * hue_roller = lv_roller_create(win, NULL);
	lv_roller_set_options(hue_roller, "0\n30\n60\n90\n120\n150\n180\n210\n240\n270\n300\n330", true);
	lv_obj_set_event_cb(hue_roller, hue_select_event_cb);

	lv_obj_t* themeLabel = lv_label_create(win, nullptr);
	lv_obj_t* hueLabel = lv_label_create(win, nullptr);
	lv_label_set_text(themeLabel, "Theme:");
	lv_label_set_text(hueLabel, "Hue:");
	lv_obj_set_pos(themeLabel, 45, 60);
	lv_obj_set_pos(hueLabel, 250, 60);
	
	lv_obj_set_pos(th_roller, 100, 20);
	lv_obj_set_pos(hue_roller, 280, 20);
	globalProps->AddObjectToWindow(themeLabel);
	globalProps->AddObjectToWindow(hueLabel);
	globalProps->AddObjectToWindow(th_roller);
	globalProps->AddObjectToWindow(hue_roller);
	cwm->AddWindow(globalProps);
}

void PropertyWindow::theme_select_event_handler(lv_obj_t * roller, lv_event_t event)
{
	if (event == LV_EVENT_VALUE_CHANGED) {
		// lv_coord_t hres = lv_disp_get_hor_res(NULL);
		// lv_coord_t vres = lv_disp_get_ver_res(NULL);

		uint16_t opt = lv_roller_get_selected(roller);
		activeTheme = themes[opt];
		lv_theme_set_current(activeTheme);
	}
}

void PropertyWindow::hue_select_event_cb(lv_obj_t * roller, lv_event_t event)
{

	if (event == LV_EVENT_VALUE_CHANGED) {
		uint16_t hue = lv_roller_get_selected(roller) * 30;

		initializeThemes(hue);

		lv_theme_set_current(activeTheme);
	}
}

void PropertyWindow::createTreeView()
{
	treeWin = new CollapsableWindow(propertyWin, "UI Objects", false, 10, 10, 400, 250);

	treeView = new TreeView(0, 0, 385, 250, "UI Objects", false);
	treeView->AddNode("Screen", lv_scr_act(), 0, true);
	treeView->AddSelectCallback(objSelectCB);
	treeView->AddDeleteCallback(deleteCB);
	
	treeWin->AddObjectToWindow(treeView->GetBaseObject());
	cwm->AddWindow(treeWin);
}

/**
 * \brief Called when a node is selected or deselected from the node tree view
 * \param node - The node that is changing state
 */
void PropertyWindow::objSelectCB(TreeNode *node)
{
	if (node->GetSelected())
	{
		// Set the node data to the node objects current style
		uiObjData *uidata = nullptr;
		try
		{
			uidata = std::any_cast<uiObjData *>(node->GetNodeData());
		} catch(std::bad_any_cast &e)
		{	
		}
		if(uidata==nullptr)
		{
			uidata = new uiObjData();
		}
		uidata->style = node->GetObject()->style_p;
		node->SetNodeData(uidata);
		lv_obj_set_style(node->GetObject(), &lv_style_plain);
	}
	else
	{
		// We are returning the node to its original style
		uiObjData *uidata = std::any_cast<uiObjData *>(node->GetNodeData());
		if(uidata==nullptr)
		{
			uidata = new uiObjData();
			return;
		}

		lv_obj_set_style(node->GetObject(), uidata->style);
	}
}


/**
 * \brief Deletes the user data from the object (If any) to clean up memory
 * \param node 
 */
void PropertyWindow::deleteCB(TreeNode* node)
{
	if(node==nullptr)
	{
		// This really should never happen
		return;
	}
	uiObjData *uiData = (uiObjData*)lv_obj_get_user_data(node->GetObject());
	if(uiData==nullptr)
	{
		return;
	}
	// Clean up any json
	if(uiData->objectJSON!=nullptr)
	{
		delete(uiData->objectJSON);
	}
}

#pragma region ThemeInit
void PropertyWindow::initializeThemes(uint16_t hue)
{
	themes.clear();
#if LV_USE_THEME_NIGHT
	themes.push_back(lv_theme_night_init(hue, NULL));
#endif

#if LV_USE_THEME_MATERIAL
	themes.push_back(lv_theme_material_init(hue, NULL));
#endif

#if LV_USE_THEME_ALIEN
	themes.push_back(lv_theme_alien_init(hue, NULL));
#endif

#if LV_USE_THEME_ZEN
	themes.push_back(lv_theme_zen_init(hue, NULL));
#endif

#if LV_USE_THEME_NEMO
	themes.push_back(lv_theme_nemo_init(hue, NULL));
#endif

#if LV_USE_THEME_MONO
	themes.push_back(lv_theme_mono_init(hue, NULL));
#endif

#if LV_USE_THEME_DEFAULT
	themes.push_back(lv_theme_default_init(hue, NULL));
#endif
}
#pragma endregion
