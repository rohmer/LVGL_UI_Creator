#include "PropertyWindow.h"
#include "ToolTray.h"
#include "Themes.h"

lv_theme_t *PropertyWindow::activeTheme;
std::vector<lv_theme_t*> PropertyWindow::themes;

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

void PropertyWindow::createPropertyWin()
{
	minWin = new MinimizableWindow("Properties", screenX - 400, 0, 400, screenY, nullptr, false, screenX/2+40, 10);
	propertyWin = minWin->GetWindowObj();
	lv_obj_set_drag(propertyWin, true);
	lv_obj_set_drag_dir(propertyWin, LV_DRAG_DIR_ALL);
	lv_obj_set_protect(propertyWin, LV_PROTECT_NONE);
	cwm = new CollapsableWindowManager(propertyWin,10,5,30,true);
	createGlobalProps();
	createBaseObjProps();
	createObjProps();
	// TreeView is the last property
	createTreeView();
}

void PropertyWindow::SetSelectedObject(lv_obj_t* object)
{
	selectedObject = object;
	updateGlobalProps();
}

void PropertyWindow::createBaseObjProps()
{
	baseObjProps = new CollapsableWindow(propertyWin, "Base Object", false, 10,0,380,350);
	lv_obj_t *boCont = lv_cont_create(baseObjProps->GetWindow(), nullptr);
	lv_cont_set_layout(boCont, LV_LAYOUT_COL_L);	
	lv_cont_set_fit(boCont, LV_FIT_FILL);	

#pragma region Position/Area
	lv_obj_t *posCont = lv_cont_create(boCont, nullptr);	
	lv_cont_set_layout(posCont, LV_LAYOUT_ROW_M);
	lv_cont_set_fit(posCont, LV_LAYOUT_GRID);
	lv_obj_set_style(posCont, &lv_style_transp);

	taX = createNumericEntry(posCont,"X");
	taY = createNumericEntry(posCont, "  Y");
	taWidth = createNumericEntry(posCont, "  Width");
	taHeight = createNumericEntry(posCont, "  Height");
#pragma endregion

#pragma region Style
	lv_obj_t *styleCont = lv_cont_create(boCont, nullptr);
	lv_cont_set_layout(styleCont, LV_LAYOUT_ROW_M);
	lv_cont_set_fit(styleCont, LV_LAYOUT_GRID);
	lv_obj_set_style(styleCont, &lv_style_transp);

	lv_obj_t *styleLab = lv_label_create(styleCont, nullptr);
	lv_label_set_text(styleLab,"Style");
	styleDD = lv_ddlist_create(styleCont, nullptr);
	std::stringstream ss;
	for(auto it=styles.begin();
		it!=styles.end();
		++it)
	{
		ss << (*it).first << "\n";
	}
	lv_ddlist_set_options(styleDD, ss.str().c_str());
	lv_ddlist_set_draw_arrow(styleDD, true);
	sInp style;
	style.name = "styledd";
	style.pw = this;
	lv_obj_set_user_data(styleDD, (lv_obj_user_data_t)&style);
	lv_obj_set_event_cb(styleDD, ddCB);	
	lv_obj_t *sLab = lv_label_create(styleCont, nullptr);
	lv_label_set_text(sLab, "New Style");	
	lv_obj_t *newSBtn = lv_btn_create(styleCont, nullptr);
	lv_obj_t *sBtnImg = lv_label_create(newSBtn, nullptr);
	lv_obj_set_size(newSBtn, 35, 35);
	lv_label_set_text(sBtnImg, LV_SYMBOL_EDIT);
	lv_obj_set_user_data(newSBtn, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(newSBtn, createStyleCB);
#pragma endregion

#pragma region Attributes
	lv_obj_t *attrCont = lv_cont_create(boCont, nullptr);
	lv_cont_set_layout(attrCont, LV_LAYOUT_ROW_M);
	lv_cont_set_fit(attrCont, LV_FIT_TIGHT);
	lv_obj_set_style(attrCont, &lv_style_transp);

	hidden = lv_cb_create(attrCont, nullptr);
	lv_cb_set_text(hidden, "Hidden");
	sInp hd;
	hd.name = "hidden";
	hd.pw = this;
	lv_obj_set_user_data(hidden, (lv_obj_user_data_t)&hd);
	lv_obj_set_event_cb(hidden, checkBoxCB);
	
	click = lv_cb_create(attrCont, nullptr);
	lv_cb_set_text(click, "Click");
	sInp cl;
	cl.name = "click";
	cl.pw = this;
	lv_obj_set_user_data(click, (lv_obj_user_data_t)&cl);
	lv_obj_set_event_cb(click, checkBoxCB);

	top = lv_cb_create(attrCont, nullptr);
	lv_cb_set_text(top, "Top");
	sInp stop;
	stop.name = "top";
	stop.pw = this;
	lv_obj_set_user_data(top, (lv_obj_user_data_t)&stop);
	lv_obj_set_event_cb(top, checkBoxCB);

	parentEvent= lv_cb_create(attrCont, nullptr);
	lv_cb_set_text(parentEvent, "Parent event");
	sInp parent;
	parent.name = "parent";
	parent.pw = this;
	lv_obj_set_user_data(parentEvent, (lv_obj_user_data_t)&parent);
	lv_obj_set_event_cb(parentEvent, checkBoxCB);

	lv_obj_t *attrCont2 = lv_cont_create(boCont, nullptr);
	lv_cont_set_layout(attrCont2, LV_LAYOUT_ROW_M);
	lv_cont_set_fit(attrCont2, LV_FIT_TIGHT);
	lv_obj_set_style(attrCont2, &lv_style_transp);

	opaScaleEnable= lv_cb_create(attrCont2, nullptr);	
	lv_cb_set_text(opaScaleEnable, "Opa Scale Enable");
	sInp oe;
	oe.name = "opaenab";
	oe.pw = this;
	lv_obj_set_user_data(opaScaleEnable, (lv_obj_user_data_t)&oe);
	lv_obj_set_event_cb(opaScaleEnable, checkBoxCB);	

	opaScale = createNumericEntry(attrCont2, "Opa Scale");
#pragma endregion

#pragma region Drag
	lv_obj_t *dragCont = lv_cont_create(boCont, nullptr);
	lv_cont_set_layout(dragCont, LV_LAYOUT_ROW_M);
	lv_cont_set_fit(dragCont, LV_LAYOUT_GRID);
	lv_obj_set_style(dragCont, &lv_style_transp);

	drag = lv_cb_create(dragCont, nullptr);
	lv_cb_set_text(drag, "Drag");
	sInp sdrag;
	sdrag.name = "drag";
	sdrag.pw = this;
	lv_obj_set_user_data(drag, (lv_obj_user_data_t)&sdrag);
	lv_obj_set_event_cb(drag, checkBoxCB);

	lv_obj_t *ddLab = lv_label_create(dragCont, nullptr);
	lv_label_set_text(ddLab, "   Dir");	
	dragDir = lv_ddlist_create(dragCont, nullptr);
	lv_ddlist_set_options(dragDir, "HOR\nVERT\nHOR&VERT");
	lv_ddlist_set_draw_arrow(dragDir, true);
	sInp sdir;
	sdir.name = "dragdir";
	sdir.pw = this;
	lv_obj_set_user_data(dragDir, (lv_obj_user_data_t)&sdir);
	lv_obj_set_event_cb(dragDir, ddCB);
	
	dragThrow = lv_cb_create(dragCont, nullptr);
	lv_cb_set_text(dragThrow, "Throw");	
	sInp dthrow;
	dthrow.name = "dragthrow";
	dthrow.pw = this;
	lv_obj_set_user_data(dragThrow, (lv_obj_user_data_t)&dthrow);
	lv_obj_set_event_cb(dragThrow, checkBoxCB);

	dragParent = lv_cb_create(dragCont, nullptr);
	lv_cb_set_text(dragParent, "Parent");
	sInp dpar;
	dpar.name = "dragparent";
	dpar.pw = this;
	lv_obj_set_user_data(dragParent, (lv_obj_user_data_t)&dpar);
	lv_obj_set_event_cb(dragParent, checkBoxCB);

#pragma endregion

#pragma region Protect
	lv_obj_t *protCont = lv_cont_create(boCont, nullptr);
	lv_obj_set_size(protCont, 360,90);
	lv_cont_set_layout(protCont, LV_LAYOUT_COL_L);
	lv_cont_set_fit(protCont, LV_FIT_NONE);
	
	lv_obj_t *protLab=lv_label_create(protCont, nullptr);
	lv_label_set_text(protLab, "Protection");

	lv_obj_t *row1 = lv_cont_create(protCont, nullptr);
	lv_obj_set_style(row1, &lv_style_transp);
	lv_cont_set_layout(row1, LV_LAYOUT_ROW_T);
	lv_cont_set_fit(row1, LV_FIT_TIGHT);
	protNone = lv_cb_create(row1, nullptr);
	lv_cb_set_text(protNone, "None");
	sInp pnone;
	pnone.name = "protnone";
	pnone.pw = this;
	lv_obj_set_user_data(protNone, (lv_obj_user_data_t)&pnone);
	lv_obj_set_event_cb(protNone, checkBoxCB);
	protPos = lv_cb_create(row1, nullptr);
	lv_cb_set_text(protPos, "Position");
	sInp ppos;
	ppos.name = "protpos";
	ppos.pw = this;
	lv_obj_set_user_data(protPos, (lv_obj_user_data_t)&ppos);
	lv_obj_set_event_cb(protPos, checkBoxCB);
	protFollow = lv_cb_create(row1, nullptr);
	lv_cb_set_text(protFollow, "Follow");
	lv_obj_set_user_data(protFollow, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(protFollow, checkBoxCB);
	protParent = lv_cb_create(row1, nullptr);
	lv_cb_set_text(protParent, "Parent");
	lv_obj_set_user_data(protParent, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(protParent, checkBoxCB);
	sInp ppar;
	ppar.name = "protpar";
	ppar.pw = this;
	lv_obj_set_user_data(protParent, (lv_obj_user_data_t)&ppar);
	lv_obj_set_event_cb(protParent, checkBoxCB);
	lv_obj_t *row2 = lv_cont_create(protCont, nullptr);
	lv_obj_set_style(row2, &lv_style_transp);
	lv_cont_set_layout(row2, LV_LAYOUT_ROW_T);
	lv_cont_set_fit(row2, LV_FIT_TIGHT);
	protPressLost = lv_cb_create(row2, nullptr);
	lv_cb_set_text(protPressLost, "Press lost");
	sInp ploss;
	ploss.name = "protpressloss";
	ploss.pw = this;
	lv_obj_set_user_data(protPressLost, (lv_obj_user_data_t)&ploss);
	lv_obj_set_event_cb(protPressLost, checkBoxCB);
	protClickFocus = lv_cb_create(row2, nullptr);
	lv_cb_set_text(protClickFocus, "Click focus");
	sInp pcf;
	pcf.name = "protclickfocus";
	pcf.pw = this;
	lv_obj_set_user_data(protClickFocus, (lv_obj_user_data_t)&pcf);
	lv_obj_set_event_cb(protClickFocus, checkBoxCB);
	protChildChg = lv_cb_create(row2, nullptr);
	lv_cb_set_text(protChildChg, "Child change");
	sInp pcc;
	pcc.name = "protchildchange";
	pcc.pw = this;
	lv_obj_set_user_data(protChildChg, (lv_obj_user_data_t)&pcc);
	lv_obj_set_event_cb(protChildChg, checkBoxCB);
	
	
#pragma endregion
	
	cwm->AddWindow(baseObjProps);
}

void PropertyWindow::updateGlobalProps()
{
	if (selectedObject == nullptr)
		return;
	sObjStruct *os =(sObjStruct*)lv_obj_get_user_data(selectedObject);
	json j = os->objectJson["base"];
	std::stringstream x, y;	
	x << j["coords"]["x1"];
	y << j["coords"]["y1"];
	lv_ta_set_text(taX,x.str().c_str());
	lv_ta_set_text(taY, y.str().c_str());
	int width = j["coords"]["x2"].get<int>() - j["coords"]["x1"].get<int>();
	int height = j["coords"]["y2"].get<int>() - j["coords"]["y1"].get<int>();
	std::stringstream ws, hs;
	ws << width;
	hs << height;
	lv_ta_set_text(taWidth, ws.str().c_str());
	lv_ta_set_text(taHeight, hs.str().c_str());

	std::string styleName = j["style"]["name"];
	int selIdx = -1;
	int i = 0;
	for(std::map<std::string, json>::iterator it=styles.begin();
		it!=styles.end();
		++it)
	{
		if (it->first == styleName)
		{
			selIdx = i;
			break;
		}
		i++;
	}
	if (selIdx != -1)
		lv_ddlist_set_selected(styleDD, selIdx);
}


lv_obj_t* PropertyWindow::createNumericEntry(lv_obj_t *parent,const std::string labelTxt)
{
	lv_obj_t *label = lv_label_create(parent , nullptr);
	lv_label_set_text(label, labelTxt.c_str());
	lv_obj_t *obj = lv_ta_create(parent, nullptr);
	lv_ta_set_one_line(obj, true);
	lv_ta_set_accepted_chars(obj, "0123456789");
	lv_ta_set_text(obj, "");
	lv_ta_set_cursor_type(obj, LV_CURSOR_NONE);
	sInp inp;
	inp.name = labelTxt;
	inp.pw = this;
	lv_obj_set_user_data(obj, (lv_obj_user_data_t)&inp);
	lv_obj_set_event_cb(obj, numericEntryCB);
	lv_obj_set_width(obj, 35);
	return obj;
}

void PropertyWindow::createObjProps()
{
	objProps = new CollapsableWindow(propertyWin, "Object", false, 10, 0, 0, 0);
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
	styles.emplace("lv_style_scr", Style::Serialize(lv_style_scr));
	styles.emplace("lv_style_transp", Style::Serialize(lv_style_transp));
	styles.emplace("lv_style_transp_tight", Style::Serialize(lv_style_transp_tight));
	styles.emplace("lv_style_transp_fit", Style::Serialize(lv_style_transp_fit));
	styles.emplace("lv_style_plain", Style::Serialize(lv_style_plain));
	styles.emplace("lv_style_plain_color", Style::Serialize(lv_style_plain_color));
	styles.emplace("lv_style_pretty", Style::Serialize(lv_style_pretty));
	styles.emplace("lv_style_pretty_color", Style::Serialize(lv_style_pretty_color));
	styles.emplace("lv_style_btn_rel", Style::Serialize(lv_style_btn_rel));
	styles.emplace("lv_style_btn_pr", Style::Serialize(lv_style_btn_pr));
	styles.emplace("lv_style_btn_tgl_rel", Style::Serialize(lv_style_btn_tgl_rel));
	styles.emplace("lv_style_btn_tgl_pr", Style::Serialize(lv_style_btn_tgl_pr));
	styles.emplace("lv_style_btn_ina", Style::Serialize(lv_style_btn_ina));
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
	if(event==LV_EVENT_CLICKED)
	{
		lv_ta_set_cursor_type(obj, LV_CURSOR_LINE);
	}
	if(event==LV_EVENT_DEFOCUSED)
	{
		lv_ta_set_cursor_type(obj, LV_CURSOR_NONE);
	}
}

void PropertyWindow::createStyleCB(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED)
	{
		// TODO: Show create style window
	}
}

void PropertyWindow::checkBoxCB(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED)
	{
		// TODO: Set the value for the checkbox
	}
}

void PropertyWindow::ddCB(lv_obj_t* obj, lv_event_t event)
{
	if(event==LV_EVENT_VALUE_CHANGED)
	{
		
	}
}

void PropertyWindow::createTreeView()
{
	treeWin = new CollapsableWindow(propertyWin, "UI Objects", false, 10, 10, 390, 250);

	treeView = new TreeView(0, 0, 385, 250, "UI Objects", false);
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
