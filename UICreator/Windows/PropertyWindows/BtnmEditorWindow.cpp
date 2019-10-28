#include "BtnmEditorWindow.h"
lv_obj_t* BtnmEditorWindow::window, * BtnmEditorWindow::preview, * BtnmEditorWindow::previewCont;
int BtnmEditorWindow::listContHeight, BtnmEditorWindow::listContWidth;
PropertyWindow* BtnmEditorWindow::pw;
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
                        bs.ctrlBits = (uint16_t)btnExt[buttons.size()].ctrl_bits;
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

void BtnmEditorWindow::closeBtnCB(lv_obj_t* obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
        lv_obj_del_async(window);
}

void BtnmEditorWindow::previewCB(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
}

uint8_t BtnmEditorWindow::get_button_width(lv_btnm_ctrl_t ctrl_bits)
{
    uint8_t w = ctrl_bits & LV_BTNM_WIDTH_MASK;
    return w != 0 ? w : 1;
}