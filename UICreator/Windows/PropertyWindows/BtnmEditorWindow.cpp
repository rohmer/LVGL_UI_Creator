#include "BtnmEditorWindow.h"
lv_obj_t* BtnmEditorWindow::window, * BtnmEditorWindow::preview, * BtnmEditorWindow::previewCont, * BtnmEditorWindow::recolor,
* BtnmEditorWindow::oneToggle, * BtnmEditorWindow::allCont, * BtnmEditorWindow::width, * BtnmEditorWindow::height,
*BtnmEditorWindow::buttonWindow;
lv_obj_t *BtnmEditorWindow::addLeft, * BtnmEditorWindow::addRight, * BtnmEditorWindow::addAbove, * BtnmEditorWindow::addBelow,
    * BtnmEditorWindow::btnLabel, *BtnmEditorWindow::delBtn, *BtnmEditorWindow::btnWidth;
lv_obj_t* BtnmEditorWindow::hidden, * BtnmEditorWindow::noRepeat, * BtnmEditorWindow::inactive, * BtnmEditorWindow::tglEnable,
* BtnmEditorWindow::tglState, * BtnmEditorWindow::clickTrig;
int BtnmEditorWindow::listContHeight, BtnmEditorWindow::listContWidth;
PropertyWindow* BtnmEditorWindow::pw;
std::vector<BtnmEditorWindow::btnStruct> BtnmEditorWindow::buttons;
bool BtnmEditorWindow::buttonEdDrawn = false;

void BtnmEditorWindow::resize()
{
    int btnmW, btnmH;
    btnmW = lv_obj_get_width(preview);
    btnmH = lv_obj_get_height(preview);

    int windowWidth;
    if (btnmW < 50)
        windowWidth = 200;
    else
        windowWidth = btnmW + 400;
    int windowHeight;
    if (btnmH < 90)
         windowHeight = 420;
    else
        windowHeight = btnmH + 330  ;
    lv_obj_set_size(window, windowWidth, windowHeight);
    int posX = lv_obj_get_width(lv_scr_act()) / 2 - windowWidth / 2;
    int posY = lv_obj_get_height(lv_scr_act()) / 2 - windowHeight / 2;
    lv_obj_set_pos(window, posX, posY);
    lv_obj_set_size(previewCont, btnmW + 10, btnmH + 30);

    lv_obj_set_pos(allCont, btnmW + 15, 5);
    lv_obj_set_size(allCont, windowWidth-btnmW-20, 90);
    lv_obj_set_pos(oneToggle, windowWidth - btnmW - 150, 20);
    if ( lv_obj_get_height(previewCont)<90)
    {
        lv_obj_set_pos(buttonWindow, 5, lv_obj_get_y(allCont)+lv_obj_get_height(allCont)+30);
    }
    else
    {
        int pY = lv_obj_get_height(previewCont);
        lv_obj_set_pos(buttonWindow, 5, lv_obj_get_height(previewCont) + lv_obj_get_y(previewCont)+5);
    }
    int val = windowHeight - btnmH - 100;
    lv_obj_set_size(buttonWindow, windowWidth - 10, 220);
}

void BtnmEditorWindow::CreateBtnmEditorWindow(PropertyWindow* propertyWindow)
{
    pw = propertyWindow;
    window = lv_win_create(lv_scr_act(), nullptr);
    lv_obj_t *cb=lv_win_add_btn(window, LV_SYMBOL_CLOSE);
    lv_obj_set_event_cb(cb, closeBtnCB);
    lv_win_set_layout(window, LV_LAYOUT_OFF);

    lv_win_set_title(window, "Button Map Editor");
    previewCont = lv_cont_create(window, nullptr);
    lv_cont_set_fit(previewCont, LV_FIT_NONE);
    preview = lv_btnm_create(previewCont, pw->GetSelectedObject());
    lv_obj_set_event_cb(preview, previewCB);

    lv_obj_set_pos(preview, 5, 25);
    lv_obj_t* l = lv_label_create(previewCont, nullptr);
    lv_label_set_text(l, "Preview");

    allCont = lv_cont_create(window, nullptr);
    recolor=lv_cb_create(allCont, nullptr);
    lv_obj_t* allContL = lv_label_create(allCont, nullptr);
    lv_label_set_text(allContL, "Button Map Settings");
    lv_obj_set_pos(allContL, 5, 5);

    lv_cb_set_text(recolor, "Recolor");
    lv_cb_set_checked(recolor,lv_btnm_get_recolor(preview));
    lv_obj_set_pos(recolor, 5, 20);

    oneToggle = lv_cb_create(allCont, nullptr);
    lv_cb_set_text(oneToggle, "One Toggle");
    lv_cb_set_checked(oneToggle, lv_btnm_get_one_toggle(preview));
    lv_obj_set_pos(oneToggle, 75, 20);
    lv_obj_set_event_cb(oneToggle, oneToggleCB);
    lv_obj_set_event_cb(recolor, recolorCB);

    lv_obj_t* sizeLab = lv_label_create(allCont, nullptr);
    lv_label_set_text(sizeLab, "Size: ");
    lv_obj_set_pos(sizeLab, 5, 60);

    lv_obj_t* xLab = lv_label_create(allCont, nullptr);
    lv_obj_t* yLab = lv_label_create(allCont, nullptr);
    lv_label_set_text(xLab, "X:");
    lv_label_set_text(yLab, "Y:");
    lv_obj_set_pos(xLab, 50, 60);
    lv_obj_set_pos(yLab, 130, 60);

    width = lv_ta_create(allCont, nullptr);
    lv_ta_set_accepted_chars(width, "0123456789");
    height = lv_ta_create(allCont, nullptr);
    lv_ta_set_accepted_chars(height, "0123456789");
    lv_obj_set_size(width, 50, 30);
    lv_obj_set_size(height, 50, 30);
    lv_obj_set_pos(width, 65, 50);
    lv_obj_set_pos(height, 145, 50);
    
    std::stringstream ss;
    ss << lv_obj_get_width(preview);
    lv_ta_set_text(width, ss.str().c_str());
    ss.str("");
    ss << lv_obj_get_height(preview);
    lv_ta_set_text(height, ss.str().c_str());
    lv_ta_set_cursor_type(width, LV_CURSOR_NONE);
    lv_ta_set_cursor_type(height, LV_CURSOR_NONE);
    lv_obj_set_user_data(width, 0);
    lv_obj_set_user_data(height, (lv_obj_user_data_t)1);

    buttonWindow = lv_cont_create(window, nullptr);
    lv_obj_t* bwLab = lv_label_create(buttonWindow, nullptr);
    lv_label_set_text(bwLab, "Button Properties");

    lv_obj_set_event_cb(width, taCB);
    lv_obj_set_event_cb(height, taCB);
    loadButtons();
    resize();
}

void BtnmEditorWindow::loadButtons()
{
    const char** m = lv_btnm_get_map_array(preview);
    int lineNum = 0;
    lv_btnm_ext_t* btnExt = (lv_btnm_ext_t*)lv_obj_get_ext_attr(preview);
    while (*m)
    {
        const char* p = *m;
        btnStruct bs;
        int i = 0;
        if (p)
            while (strlen(p) > 0)
            {
                if (p == "\n")
                {
                    lineNum++;
                    i = 0;
                }
                else
                {
                    std::stringstream ss;
                    ss << p;
                    switch (i)
                    {
                    case(0):
                        bs.btnName = ss.str();
                        // Get width and ctrl bits
                        bs.ctrlBits = (uint16_t)(btnExt[buttons.size()].ctrl_bits);
                        bs.width = get_button_width(bs.ctrlBits);
                        break;
                    case(1):
                        bs.tn = ss.str();
                        break;
                    case(2):
                        bs.n = ss.str();
                        break;
                    case(3):
                        bs.btnNum = atoi(ss.str().c_str());
                        break;
                    }
                    bs.lineNum = lineNum;
                }
                i++;
                p++;
            }
        if (bs.btnName.length() > 0)
            buttons.push_back(bs);
        m++;
    }
    
    
}

void BtnmEditorWindow::createButtonEd()
{
    addLeft = lv_btn_create(buttonWindow, nullptr);
    lv_obj_t* l1 = lv_label_create(addLeft, nullptr);
    std::stringstream ss;
    ss << LV_SYMBOL_LEFT << "Add Btn to Left";
    lv_label_set_text(l1, ss.str().c_str());
    ss.str("");
    lv_obj_set_size(addLeft, 100, 30);
    int mid = 50;
    lv_obj_set_pos(addLeft, 5, mid);

    addAbove = lv_btn_create(buttonWindow, nullptr);
    lv_obj_t* l2 = lv_label_create(addAbove, nullptr);
    ss << LV_SYMBOL_UP << "Add Btn Above";
    lv_label_set_text(l2, ss.str().c_str());
    ss.str("");
    lv_obj_set_size(addAbove, 100, 30);
    lv_obj_set_pos(addAbove, 130, 15);

    btnLabel = lv_ta_create(buttonWindow, nullptr);
    lv_obj_set_size(btnLabel, 100, 30);
    lv_obj_set_pos(btnLabel, 120, mid);
    lv_obj_set_user_data(btnLabel, (lv_obj_user_data_t)2);
    lv_obj_set_event_cb(btnLabel, taCB);
    lv_ta_set_cursor_type(btnLabel, LV_CURSOR_NONE);

    delBtn = lv_btn_create(buttonWindow, nullptr);
    lv_obj_t* l3 = lv_label_create(delBtn, nullptr);
    lv_label_set_text(l3, LV_SYMBOL_TRASH);
    lv_obj_set_size(delBtn, 30, 30);
    lv_obj_set_pos(delBtn, 225, mid);

    addRight = lv_btn_create(buttonWindow, nullptr);
    lv_obj_t* l4 = lv_label_create(addRight, nullptr);
    ss << LV_SYMBOL_RIGHT << "Add Btn to Right";
    lv_label_set_text(l4, ss.str().c_str());
    ss.str("");
    lv_obj_set_size(addRight, 100, 30);
    lv_obj_set_pos(addRight, 270, mid);

    addBelow = lv_btn_create(buttonWindow, nullptr);
    lv_obj_t* l5 = lv_label_create(addBelow, nullptr);
    ss << LV_SYMBOL_DOWN << "Add Btn Below";
    lv_label_set_text(l5, ss.str().c_str());
    ss.str("");
    lv_obj_set_size(addBelow, 100, 30);
    lv_obj_set_pos(addBelow, 130, mid+40);

    lv_obj_t* l6 = lv_label_create(buttonWindow, nullptr);
    lv_label_set_text(l6, "Button Width:");
    lv_obj_set_pos(l6, lv_obj_get_width(buttonWindow) - 140,20);

    btnWidth = lv_ta_create(buttonWindow, nullptr);
    lv_ta_set_cursor_type(btnWidth, LV_CURSOR_NONE);
    lv_obj_set_size(btnWidth, 30, 25);
    lv_obj_set_pos(btnWidth, lv_obj_get_width(buttonWindow) - 50, 10);
    lv_ta_set_cursor_type(btnWidth, LV_CURSOR_NONE);
    lv_obj_set_user_data(btnLabel, (lv_obj_user_data_t)3);
    lv_ta_set_accepted_chars(btnWidth, "0123456789");

    lv_obj_t* bOptsWin = lv_cont_create(buttonWindow, nullptr);
    lv_obj_set_pos(bOptsWin, 5, mid + 80);
    lv_obj_set_size(bOptsWin, lv_obj_get_width(buttonWindow) - 10, 85);
    lv_cont_set_fit(bOptsWin, LV_FIT_NONE);
    lv_cont_set_layout(bOptsWin, LV_LAYOUT_PRETTY);

    hidden = lv_cb_create(bOptsWin, nullptr);
    lv_cb_set_text(hidden,"Hidden");
    noRepeat = lv_cb_create(bOptsWin, nullptr);
    lv_cb_set_text(noRepeat, "No Repeat");
    inactive = lv_cb_create(bOptsWin, nullptr);
    lv_cb_set_text(inactive, "Inactive");
    tglEnable = lv_cb_create(bOptsWin, nullptr);
    lv_cb_set_text(tglEnable, "Toggle Enable");
    tglState = lv_cb_create(bOptsWin, nullptr);
    lv_cb_set_text(tglState, "Toggle State");
    clickTrig = lv_cb_create(bOptsWin, nullptr);
    lv_cb_set_text(clickTrig, "Click Trigger");

}

void BtnmEditorWindow::updateButtonEd(int buttonNum)
{
    if (!buttonEdDrawn)
        createButtonEd();
    buttonEdDrawn = true;
    btnStruct bs = buttons[buttonNum];
    lv_ta_set_text(btnLabel, bs.btnName.c_str());
    std::stringstream ss;
    ss << bs.width;
    lv_ta_set_text(btnWidth, ss.str().c_str());

    bool bHid = bs.ctrlBits && LV_BTNM_CTRL_HIDDEN;
    bool bNoRep = bs.ctrlBits && LV_BTNM_CTRL_NO_REPEAT;
    bool bInac = bs.ctrlBits && LV_BTNM_CTRL_INACTIVE;
    bool bTglEn = bs.ctrlBits && LV_BTNM_CTRL_INACTIVE;
    bool bTglSt = bs.ctrlBits && LV_BTNM_CTRL_TGL_STATE;
    bool bClkTrig = bs.ctrlBits && LV_BTNM_CTRL_CLICK_TRIG;

    lv_cb_set_checked(hidden, bHid);
    lv_cb_set_checked(noRepeat, bNoRep);
    lv_cb_set_checked(inactive, bInac);
    lv_cb_set_checked(tglEnable, bTglEn);
    lv_cb_set_checked(tglState, bTglSt);
    lv_cb_set_checked(clickTrig, bClkTrig);

}

void BtnmEditorWindow::closeBtnCB(lv_obj_t* obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
        lv_obj_del_async(window);
}

void BtnmEditorWindow::previewCB(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_VALUE_CHANGED)
        return;
    int buttonNum=lv_btnm_get_active_btn(preview);
    updateButtonEd(buttonNum);
}

void BtnmEditorWindow::recolorCB(lv_obj_t* obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        lv_btnm_set_recolor(preview, lv_cb_is_checked(obj));
    }
}

void BtnmEditorWindow::oneToggleCB(lv_obj_t* obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        lv_btnm_set_one_toggle(preview, lv_cb_is_checked(obj));
    }
}

void BtnmEditorWindow::taCB(lv_obj_t* obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        lv_ta_set_cursor_type(obj, LV_CURSOR_LINE);
        lv_group_t* kbGroup = pw->GetKBGroup();
        lv_group_remove_all_objs(kbGroup);
        lv_group_add_obj(kbGroup, obj);
        return;
    }
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        std::string val = lv_ta_get_text(obj);
        if (val.empty())
            return;
        int value = atoi(val.c_str());
        if (value == 0)
            return;
        int i = (int)lv_obj_get_user_data(obj);
        if (i == 0)
            lv_obj_set_width(preview, value);
        if (i == 1)
            lv_obj_set_height(preview, value);

        resize();
    }
}

uint8_t BtnmEditorWindow::get_button_width(lv_btnm_ctrl_t ctrl_bits)
{
    uint8_t w = ctrl_bits & LV_BTNM_WIDTH_MASK;
    return w != 0 ? w : 1;
}