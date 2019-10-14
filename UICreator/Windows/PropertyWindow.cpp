#include "PropertyWindow.h"
#include "ToolBar.h"
#include "ToolTray.h"
#include "Themes.h"

lv_theme_t *PropertyWindow::activeTheme;
std::vector<lv_theme_t*> PropertyWindow::themes;
bool PropertyWindow::drawing = false;

PropertyWindow::PropertyWindow(SimWindow* simWindow, int screenWidth, int screenHeight) :
	simWindow(simWindow),
	screenX(screenWidth),
	screenY(screenHeight)
{
	initializeThemes(0);
	initStyles();
	createPropertyWin();
}

TreeView* PropertyWindow::GetObjectTree()
{
	return treeView;
}

lv_obj_t* PropertyWindow::GetSelectedObject()
{
	return selectedObject;
}

void PropertyWindow::createPropertyWin()
{
	minWin = new MinimizableWindow("Properties", screenX - 400, 0, 400, screenY, nullptr, false, screenX/2+40, 10);
	propertyWin = minWin->GetWindowObj();
	lv_obj_set_drag(propertyWin, true);
	lv_obj_set_drag_dir(propertyWin, LV_DRAG_DIR_ALL);
	lv_obj_set_protect(propertyWin, LV_PROTECT_NONE);
	cwm = new CollapsableWindowManager(propertyWin,10,5,30,true);
	createGlobalProps();
    BaseProperties::CreateBaseObjProps(this);
    createObjProps();
	// TreeView is the last property
	createTreeView();
}

void PropertyWindow::SetSelectedObject(lv_obj_t* object)
{
	selectedObject = object;
	json j = Serialization::ObjectSerializer::SerializeObject(object);    
    ObjectUserData* objectData = (ObjectUserData*)lv_obj_get_user_data(object);
    objectData->objectJson = j;
    lv_obj_set_user_data(object, objectData);
	BaseProperties::UpdateGlobalProps(this,j);
	if(j["arc"].is_object())
	{
		ArcProperties::UpdateArcProperties(this, j);
	}
}

void PropertyWindow::createObjProps()
{
	objProps = new CollapsableWindow(propertyWin, "Object", false, 10, 0, 375, 0);
	cwm->AddWindow(objProps);
}

void PropertyWindow::createGlobalProps()
{
	globalProps = new CollapsableWindow(propertyWin, "Global", false, 10, 0, 400, 75);
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
	lv_obj_set_pos(themeLabel, 45, 20);
	lv_obj_set_pos(hueLabel, 250, 20);
	
	lv_obj_set_pos(th_roller, 100, 0);
	lv_obj_set_pos(hue_roller, 280, 0);
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

void PropertyWindow::initStyles()
{
	using namespace Serialization;
	Styles.emplace("lv_style_scr", Style::ToJSON(lv_style_scr));
	Styles.emplace("lv_style_transp", Style::ToJSON(lv_style_transp));
	Styles.emplace("lv_style_transp_tight", Style::ToJSON(lv_style_transp_tight));
	Styles.emplace("lv_style_transp_fit", Style::ToJSON(lv_style_transp_fit));
	Styles.emplace("lv_style_plain", Style::ToJSON(lv_style_plain));
	Styles.emplace("lv_style_plain_color", Style::ToJSON(lv_style_plain_color));
	Styles.emplace("lv_style_pretty", Style::ToJSON(lv_style_pretty));
	Styles.emplace("lv_style_pretty_color", Style::ToJSON(lv_style_pretty_color));
	Styles.emplace("lv_style_btn_rel", Style::ToJSON(lv_style_btn_rel));
	Styles.emplace("lv_style_btn_pr", Style::ToJSON(lv_style_btn_pr));
	Styles.emplace("lv_style_btn_tgl_rel", Style::ToJSON(lv_style_btn_tgl_rel));
	Styles.emplace("lv_style_btn_tgl_pr", Style::ToJSON(lv_style_btn_tgl_pr));
	Styles.emplace("lv_style_btn_ina", Style::ToJSON(lv_style_btn_ina));
}

void PropertyWindow::hue_select_event_cb(lv_obj_t * roller, lv_event_t event)
{

	if (event == LV_EVENT_VALUE_CHANGED) {
		uint16_t hue = lv_roller_get_selected(roller) * 30;

		initializeThemes(hue);

		lv_theme_set_current(activeTheme);
	}
}

void PropertyWindow::numericEntryCB(lv_obj_t* obj, lv_event_t event)
{
	if (drawing)
		return;
	if(event==LV_EVENT_CLICKED)
	{
		lv_ta_set_cursor_type(obj, LV_CURSOR_LINE);
	}
	if(event==LV_EVENT_DEFOCUSED)
	{
		lv_ta_set_cursor_type(obj, LV_CURSOR_NONE);
	}
	if(event==LV_EVENT_VALUE_CHANGED)
	{
		sPropChange *pc = (sPropChange*)lv_obj_get_user_data(obj);
		ObjectUserData *oud = (ObjectUserData*)lv_obj_get_user_data(pc->pw->selectedObject);
		json j = oud->objectJson;		
		j[nlohmann::json_pointer<std::string>(pc->propertyPath)] = atoi(lv_ta_get_text(obj));

		std::vector<std::string> propTokens = ObjectTools::Split(pc->propertyPath, '/');
		std::string oType = propTokens[0];
		if(oType=="base")
		{
			if (!Serialization::LVObject::SetValue(pc->pw->selectedObject, pc->propertyPath, atoi(lv_ta_get_text(obj))))
			{
				spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
			}
		}
		if(oType=="arc")
		{
			if (!Serialization::LVArc::SetValue(pc->pw->selectedObject, pc->propertyPath, atoi(lv_ta_get_text(obj))))
			{
				spdlog::get("console")->error("Failed to set value: {0}", pc->propertyPath);
			}
		}

		oud->objectJson = j;
		lv_obj_set_user_data(obj, (lv_obj_user_data_t)oud);
	}
}

void PropertyWindow::createTreeView()
{
	treeWin = new CollapsableWindow(propertyWin, "UI Objects", false, 10, 10, 390, 250);

	treeView = new TreeView(0, 0, 385, 250, "UI Objects", false, false);
	treeView->AddNode("Screen", lv_scr_act(), 0, true);
	treeView->AddSelectCallback(objSelectCB);
	treeView->AddDeleteCallback(deleteCB);
	
	treeWin->AddObjectToWindow(treeView->GetTreeViewUI());
	cwm->AddWindow(treeWin);
}

/**
 * \brief Called when a node is selected or deselected from the node tree view
 * \param node - The node that is changing state
 */
void PropertyWindow::objSelectCB(TreeNode *node)
{
	if (node->IsSelected())
	{
		// Set the node data to the node objects current style
		uiObjData *uidata = nullptr;
		try
		{
			uidata = std::any_cast<uiObjData *>(node->GetNodeData());
		} catch(std::bad_any_cast)
		{	
		}
		if(uidata==nullptr)
		{
			uidata = new uiObjData();
		}
		uidata->style = node->GetLVObject()->style_p;
		node->SetNodeData(uidata);
		lv_obj_set_style(node->GetLVObject(), &lv_style_plain);
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

		lv_obj_set_style(node->GetLVObject(), uidata->style);
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
	uiObjData *uiData = (uiObjData*)lv_obj_get_user_data(node->GetLVObject());
	if(uiData==nullptr)
	{
		return;
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

