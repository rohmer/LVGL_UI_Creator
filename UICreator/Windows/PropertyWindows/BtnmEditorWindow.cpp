#include "BtnmEditorWindow.h"
lv_obj_t* BtnmEditorWindow::window, * BtnmEditorWindow::preview, * BtnmEditorWindow::previewCont,
* BtnmEditorWindow::mapTA;
std::string BtnmEditorWindow::btnMap;
int BtnmEditorWindow::listContHeight, BtnmEditorWindow::listContWidth;
PropertyWindow* BtnmEditorWindow::pw;
CollapsableWindowManager* BtnmEditorWindow::cwm;
CollapsableWindow* BtnmEditorWindow::defWin;
std::vector<BtnmEditorWindow::btnStruct> BtnmEditorWindow::buttons;

void BtnmEditorWindow::resize()
{
    int btnmW, btnmH;
    lv_obj_t* btnm = pw->GetSelectedObject();
    btnmW = lv_obj_get_width(btnm);
    btnmH = lv_obj_get_height(btnm);

    int windowWidth;
    if (btnmW < 50)
        windowWidth = 200;
    else
        windowWidth = btnmW + 350;
    int windowHeight;
    if (btnmH < 350)
        windowHeight = 350;
    else
        windowHeight = btnmH + 25;
    lv_obj_set_size(window, windowWidth, windowHeight);
    int posX = lv_obj_get_width(lv_scr_act()) / 2 - windowWidth / 2;
    int posY = lv_obj_get_height(lv_scr_act()) / 2 - windowHeight / 2;
    lv_obj_set_pos(window, posX, posY);
    lv_obj_set_size(previewCont, btnmW + 10, btnmH + 30);
    listContWidth = windowWidth - lv_obj_get_width(previewCont) - 20;
    listContHeight = windowHeight / 2;
    cwm->SetPos(btnmW + 20, 5);
    cwm->SetWidth(listContWidth-16);
    lv_obj_set_size(mapTA, lv_obj_get_width(defWin->GetWindow()) - 10, lv_obj_get_height(defWin->GetWindow()) - 40);
    lv_obj_set_pos(mapTA, 5, 5);

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
    lv_obj_set_pos(preview, 5, 25);
    lv_obj_t* l = lv_label_create(previewCont, nullptr);
    lv_label_set_text(l, "Preview");
    cwm = new CollapsableWindowManager(window);
    defWin = new CollapsableWindow(window, "Button Map", true, 300, 150);
    cwm->AddWindow(defWin);
    mapTA = lv_ta_create(defWin->GetWindow(), nullptr);
    lv_ta_set_one_line(mapTA, false);
    
    lv_obj_t* updateButton = lv_btn_create(defWin->GetWindow(), nullptr);
    lv_obj_t* updateButtonL = lv_label_create(updateButton, nullptr);
    lv_label_set_text(updateButtonL, "Update Button Map");
    lv_obj_set_event_cb(updateButtonL, assignMap);
    lv_obj_set_size(updateButton, 275, 30);
    lv_obj_set_y(updateButton, lv_obj_get_height(defWin->GetWindow()) - 30);
    lv_obj_set_x(updateButton, 10);
    updateMap();
    
    resize();
}

void BtnmEditorWindow::updateMap()
{
    buttons.clear();
    const char** m = lv_btnm_get_map_array(preview);
    int lineNum = 0;
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

    std::stringstream mapStr;
    bool first = true;
    lineNum = 0;
    for (int i = 0; i < buttons.size(); i++)
    {
        if (lineNum < buttons[i].lineNum)
        {
            mapStr << "\n";
            lineNum = buttons[i].lineNum;
        }
        else
        {
            if(!first)
                mapStr << ",";
            first = false;
        }
        mapStr << buttons[i].btnName;
    }
    lv_ta_set_text(mapTA, mapStr.str().c_str());
}

void BtnmEditorWindow::closeBtnCB(lv_obj_t* obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
        lv_obj_del_async(window);
}

void BtnmEditorWindow::assignMap(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    std::string m = lv_ta_get_text(mapTA);
    std::vector<std::string> lines=
}
