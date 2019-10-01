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
	createBaseObjProps();
	createObjProps();
	// TreeView is the last property
	createTreeView();
}

void PropertyWindow::SetSelectedObject(lv_obj_t* object)
{
	selectedObject = object;
	json j = Serialization::LVObject::ToJSON(object);
	updateGlobalProps();
	if(j["arc"].is_object())
	{
		updateArcProperties();
	}
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

	taX = createNumericEntry(posCont,"X1", "/base/coords/x1");
	taY = createNumericEntry(posCont, "Y1","/base/coords/y1");
	taWidth = createNumericEntry(posCont, "X2","/base/coords/x2");
	taHeight = createNumericEntry(posCont, "Y2", "/base/coords/y2");
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
	sPropChange *pc1 = new sPropChange();
	pc1->pw = this;
	pc1->propertyPath = "/base/style";
	lv_obj_set_user_data(styleDD, (lv_obj_user_data_t)pc1);
	lv_obj_set_event_cb(styleDD, ddListCB);	
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
	sPropChange *pc2 = new sPropChange();
	pc2->pw = this;
	pc2->propertyPath = "/base/hidden";
	lv_obj_set_user_data(hidden, (lv_obj_user_data_t)pc2);
	lv_obj_set_event_cb(hidden, checkBoxCB);
	
	click = lv_cb_create(attrCont, nullptr);
	lv_cb_set_text(click, "Click");
	sPropChange *pc3 = new sPropChange();
	pc3->pw = this;
	pc3->propertyPath = "/base/click";
	lv_obj_set_user_data(click, (lv_obj_user_data_t)pc3);
	lv_obj_set_event_cb(click, checkBoxCB);

	top = lv_cb_create(attrCont, nullptr);
	lv_cb_set_text(top, "Top");
	sPropChange *pc4 = new sPropChange();
	pc4->pw = this;
	pc4->propertyPath = "/base/top";

	lv_obj_set_user_data(top, (lv_obj_user_data_t)pc4);
	lv_obj_set_event_cb(top, checkBoxCB);

	parentEvent= lv_cb_create(attrCont, nullptr);
	lv_cb_set_text(parentEvent, "Parent event");
	sPropChange *pc5 = new sPropChange();
	pc5->pw = this;
	pc5->propertyPath = "/base/parevent";
	lv_obj_set_user_data(parentEvent, (lv_obj_user_data_t)pc5);
	lv_obj_set_event_cb(parentEvent, checkBoxCB);

	lv_obj_t *attrCont2 = lv_cont_create(boCont, nullptr);
	lv_cont_set_layout(attrCont2, LV_LAYOUT_ROW_M);
	lv_cont_set_fit(attrCont2, LV_FIT_TIGHT);
	lv_obj_set_style(attrCont2, &lv_style_transp);

	opaScaleEnable= lv_cb_create(attrCont2, nullptr);	
	lv_cb_set_text(opaScaleEnable, "Opa Scale Enable");
	sPropChange *pc6 = new sPropChange();
	pc6->pw = this;
	pc6->propertyPath = "/base/opascaleen";

	lv_obj_set_user_data(opaScaleEnable, (lv_obj_user_data_t)pc6);
	lv_obj_set_event_cb(opaScaleEnable, checkBoxCB);	

	opaScale = createNumericEntry(attrCont2, "Opa Scale", "/base/opascale");

#pragma endregion

#pragma region Drag
	lv_obj_t *dragCont = lv_cont_create(boCont, nullptr);
	lv_cont_set_layout(dragCont, LV_LAYOUT_ROW_M);
	lv_cont_set_fit(dragCont, LV_LAYOUT_GRID);
	lv_obj_set_style(dragCont, &lv_style_transp);

	drag = lv_cb_create(dragCont, nullptr);
	lv_cb_set_text(drag, "Drag");
	sPropChange *pc7 = new sPropChange();
	pc7->pw = this;
	pc7->propertyPath = "/base/drag";
	lv_obj_set_user_data(drag, (lv_obj_user_data_t)pc7);
	lv_obj_set_event_cb(drag, checkBoxCB);

	lv_obj_t *ddLab = lv_label_create(dragCont, nullptr);
	lv_label_set_text(ddLab, "   Dir");	
	dragDir = lv_ddlist_create(dragCont, nullptr);
	lv_ddlist_set_options(dragDir, "HOR\nVERT\nALL");
	lv_ddlist_set_draw_arrow(dragDir, true);
	sPropChange *pc8 = new sPropChange();
	pc8->pw = this;
	pc8->propertyPath = "/base/dragDir";
	lv_obj_set_user_data(dragDir, (lv_obj_user_data_t)pc8);
	lv_obj_set_event_cb(dragDir, ddListCB);
	
	dragThrow = lv_cb_create(dragCont, nullptr);
	lv_cb_set_text(dragThrow, "Throw");	
	sPropChange *pc9 = new sPropChange();
	pc9->pw = this;
	pc9->propertyPath = "/base/dragThrow";

	lv_obj_set_user_data(dragThrow, (lv_obj_user_data_t)pc9);
	lv_obj_set_event_cb(dragThrow, checkBoxCB);

	dragParent = lv_cb_create(dragCont, nullptr);
	lv_cb_set_text(dragParent, "Parent");
	sPropChange *pc10 = new sPropChange();
	pc10->pw = this;
	pc10->propertyPath = "/base/dragParent";
	lv_obj_set_user_data(dragParent, (lv_obj_user_data_t)pc10);
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
	sPropChange *pc11 = new sPropChange();
	pc11->pw = this;
	pc11->propertyPath = "/base/protect/protNone";
	lv_obj_set_user_data(protNone, (lv_obj_user_data_t)pc11);
	lv_obj_set_event_cb(protNone, checkBoxCB);
	protPos = lv_cb_create(row1, nullptr);
	lv_cb_set_text(protPos, "Position");
	sPropChange *pc12 = new sPropChange();
	pc12->pw = this;
	pc12->propertyPath = "/base/protect/protPos";
	lv_obj_set_user_data(protPos, (lv_obj_user_data_t)pc12);
	lv_obj_set_event_cb(protPos, checkBoxCB);
	protFollow = lv_cb_create(row1, nullptr);
	sPropChange *pc17 = new sPropChange();
	pc17->pw = this;
	pc17->propertyPath = "/base/protect/protFollow";
	lv_cb_set_text(protFollow, "Follow");
	lv_obj_set_user_data(protFollow, (lv_obj_user_data_t)pc17);
	lv_obj_set_event_cb(protFollow, checkBoxCB);
	protParent = lv_cb_create(row1, nullptr);
	lv_cb_set_text(protParent, "Parent");
	sPropChange *pc13 = new sPropChange();
	pc13->pw = this;
	pc13->propertyPath = "/base/protect/protParent";
	lv_obj_set_user_data(protParent, (lv_obj_user_data_t)pc13);
	lv_obj_set_event_cb(protParent, checkBoxCB);
	lv_obj_t *row2 = lv_cont_create(protCont, nullptr);
	lv_obj_set_style(row2, &lv_style_transp);
	lv_cont_set_layout(row2, LV_LAYOUT_ROW_T);
	lv_cont_set_fit(row2, LV_FIT_TIGHT);
	protPressLost = lv_cb_create(row2, nullptr);
	lv_cb_set_text(protPressLost, "Press lost");
	sPropChange *pc14 = new sPropChange();
	pc14->pw = this;
	pc14->propertyPath = "/base/protect/protPressLost";

	lv_obj_set_user_data(protPressLost, (lv_obj_user_data_t)pc14);
	lv_obj_set_event_cb(protPressLost, checkBoxCB);
	protClickFocus = lv_cb_create(row2, nullptr);
	lv_cb_set_text(protClickFocus, "Click focus");
	sPropChange *pc15 = new sPropChange();
	pc15->pw = this;
	pc15->propertyPath = "/base/protect/protClickFocus";
	lv_obj_set_user_data(protClickFocus, (lv_obj_user_data_t)pc15);
	lv_obj_set_event_cb(protClickFocus, checkBoxCB);
	protChildChg = lv_cb_create(row2, nullptr);
	lv_cb_set_text(protChildChg, "Child change");
	sPropChange *pc16 = new sPropChange();
	pc16->pw = this;
	pc16->propertyPath = "/base/protect/protChildChange";
	lv_obj_set_user_data(protChildChg, (lv_obj_user_data_t)pc16);
	lv_obj_set_event_cb(protChildChg, checkBoxCB);
		
#pragma endregion
	cwm->AddWindow(baseObjProps);
}

void PropertyWindow::updateGlobalProps()
{
	if (selectedObject == nullptr)
		return;
	drawing = true;
	ObjectUserData* oud = (ObjectUserData*)lv_obj_get_user_data(selectedObject);
	json j = oud->objectJson;
	std::stringstream ss;
	std::stringstream x, y;
	json bj = j["base"];
	x << bj["coords"]["x1"];
	y << j["coords"]["y1"];
	lv_ta_set_text(taX,x.str().c_str());
	lv_ta_set_text(taY, y.str().c_str());
	int width = bj["coords"]["x2"].get<int>() - bj["coords"]["x1"].get<int>();
	int height = bj["coords"]["y2"].get<int>() - bj["coords"]["y1"].get<int>();
	std::stringstream ws, hs;
	ws << width;
	hs << height;
	lv_ta_set_text(taWidth, ws.str().c_str());
	lv_ta_set_text(taHeight, hs.str().c_str());

	std::string styleName = bj["style"]["name"];
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
	lv_cb_set_checked(drag, bj["drag"]);
	lv_ddlist_set_selected(dragDir, bj["dragDir"]);
	lv_cb_set_checked(dragThrow, bj["dragThrow"]);

	lv_cb_set_checked(hidden, bj["hidden"]);
	lv_cb_set_checked(click, bj["click"]);
	lv_cb_set_checked(top, bj["top"]);
	lv_cb_set_checked(parentEvent, bj["parevent"]);
	lv_cb_set_checked(opaScaleEnable, bj["opascaleen"]);

	std::stringstream oss;
	oss << bj["opascale"];
	lv_ta_set_text(opaScale, oss.str().c_str());
	if(bj["protect"]["protNone"])
	{
		lv_cb_set_checked(protNone, true);
		lv_cb_set_checked(protPos, false);
		lv_cb_set_checked(protFollow, false);
		lv_cb_set_checked(protParent, false);
		lv_cb_set_checked(protPressLost, false);
		lv_cb_set_checked(protClickFocus, false);
		lv_cb_set_checked(protChildChg, false);		
	}
	if (bj["protect"]["protChildChange"])
		lv_cb_set_checked(protChildChg, true);
	else
		lv_cb_set_checked(protChildChg, false);
	if (bj["protect"]["protParent"])
		lv_cb_set_checked(protParent, true);
	else
		lv_cb_set_checked(protParent, false);
	if (bj["protect"]["protPos"])
		lv_cb_set_checked(protPos, true);
	else
		lv_cb_set_checked(protPos, false);
	if (bj["protect"]["protFollow"])
		lv_cb_set_checked(protFollow, true);
	else
		lv_cb_set_checked(protFollow, false);
	if (bj["protect"]["protPressLost"])
		lv_cb_set_checked(protPressLost, true);
	else
		lv_cb_set_checked(protPressLost, false);
	if (bj["protect"]["protClickFocus"])
		lv_cb_set_checked(protClickFocus, true);
	else
		lv_cb_set_checked(protClickFocus, false);
	drawing = false;
}

void PropertyWindow::createArcProperties()
{
	objProps->UpdateHeight(300);
	lv_obj_t *cont = lv_cont_create(objProps->GetWindow(), nullptr);
	lv_cont_set_layout(cont, LV_LAYOUT_PRETTY);
	lv_cont_set_fit(cont, LV_FIT_FILL);

	arcStartTA=createNumericEntry(cont, "Arc Start","/arc/angle/start");
	arcEndTA = createNumericEntry(cont, "Arc End","/arc/angle/end");

	lv_obj_t *lineProps = lv_cont_create(cont, nullptr);
	lv_cont_set_layout(lineProps, LV_LAYOUT_PRETTY);
	lv_obj_set_size(lineProps, 370, 300);
	lv_obj_t *lineLab = lv_label_create(lineProps, nullptr);
	lv_label_set_text(lineLab, "Line Properties:");
	lv_label_set_align(lineLab, LV_LABEL_ALIGN_LEFT);
	ColorPicker *cp = new ColorPicker(0, 0, 375, 150, 0x51F542, lineProps);
	sOData odata;
	odata.pw = this;
	odata.objName = "ArcColor";
	cp->SetCallback(assignColor, odata);
	arcLineWidth = createNumericEntry(lineProps, "Line Width","/arc/style/line.width");
	arcLineRound = lv_cb_create(lineProps, nullptr);
	lv_cb_set_text(arcLineRound, "Arc Line Rounded");
	sPropChange *objData=new sPropChange();
	objData->pw = this;
	objData->propertyPath = "/arc/style/line.rounded";
	lv_obj_set_user_data(arcLineRound, (lv_obj_user_data_t)&objData);
	lv_obj_set_event_cb(arcLineRound, checkBoxCB);
	cwm->Update();
}

void PropertyWindow::updateArcProperties()
{
	if(currentlyLoadedProp!=eObjType::ARC)
	{
		objProps->DeleteChildren();
		createArcProperties();
	}
}

void PropertyWindow::assignColor(lv_color_t color, std::any objectData)
{
	sOData odata=std::any_cast<sOData>(objectData);
	if(odata.objName=="ArcColor")
	{
		static lv_style_t newStyle;
		lv_style_copy(&newStyle, &lv_style_plain);

		int lineWidth = std::atoi(lv_ta_get_text(odata.pw->arcLineWidth));
		newStyle.line.width = lineWidth;
		color.ch.alpha = 255;
		newStyle.line.color = color;
		if (lv_cb_is_checked(odata.pw->arcLineRound))
			newStyle.line.rounded = true;
		else
			newStyle.line.rounded = false;
		lv_arc_set_style(odata.pw->selectedObject, LV_ARC_STYLE_MAIN, &newStyle);
	}
}

lv_obj_t* PropertyWindow::createNumericEntry(lv_obj_t *parent,const std::string labelTxt, const std::string path)
{
	lv_obj_t *label = lv_label_create(parent , nullptr);
	lv_label_set_text(label, labelTxt.c_str());
	lv_obj_t *obj = lv_ta_create(parent, nullptr);
	lv_ta_set_one_line(obj, true);
	lv_ta_set_accepted_chars(obj, "0123456789");
	lv_ta_set_text(obj, "");
	lv_ta_set_cursor_type(obj, LV_CURSOR_NONE);
	sPropChange *propChange = new sPropChange();
	propChange->pw = this;
	propChange->propertyPath = path;
	lv_obj_set_user_data(obj, (lv_obj_user_data_t)propChange);
	lv_obj_set_event_cb(obj, numericEntryCB);
	lv_obj_set_width(obj, 35);
	return obj;
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
	styles.emplace("lv_style_scr", Style::ToJSON(lv_style_scr));
	styles.emplace("lv_style_transp", Style::ToJSON(lv_style_transp));
	styles.emplace("lv_style_transp_tight", Style::ToJSON(lv_style_transp_tight));
	styles.emplace("lv_style_transp_fit", Style::ToJSON(lv_style_transp_fit));
	styles.emplace("lv_style_plain", Style::ToJSON(lv_style_plain));
	styles.emplace("lv_style_plain_color", Style::ToJSON(lv_style_plain_color));
	styles.emplace("lv_style_pretty", Style::ToJSON(lv_style_pretty));
	styles.emplace("lv_style_pretty_color", Style::ToJSON(lv_style_pretty_color));
	styles.emplace("lv_style_btn_rel", Style::ToJSON(lv_style_btn_rel));
	styles.emplace("lv_style_btn_pr", Style::ToJSON(lv_style_btn_pr));
	styles.emplace("lv_style_btn_tgl_rel", Style::ToJSON(lv_style_btn_tgl_rel));
	styles.emplace("lv_style_btn_tgl_pr", Style::ToJSON(lv_style_btn_tgl_pr));
	styles.emplace("lv_style_btn_ina", Style::ToJSON(lv_style_btn_ina));
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

void PropertyWindow::createStyleCB(lv_obj_t* obj, lv_event_t event)
{	
	if (event == LV_EVENT_CLICKED)
	{
		// TODO: Show create style window
	}
}

void PropertyWindow::checkBoxCB(lv_obj_t* obj, lv_event_t event)
{
	if (drawing)
		return;
	if (event == LV_EVENT_CLICKED)
	{
		// TODO: Set the value for the checkbox
	}
}

void PropertyWindow::ddListCB(lv_obj_t* obj, lv_event_t event)
{
	if (drawing)
		return;
	if (event == LV_EVENT_VALUE_CHANGED)
	{
		// TODO: Set the value for the ddList
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

