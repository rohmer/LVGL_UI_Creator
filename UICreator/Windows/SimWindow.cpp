#include "SimWindow.h"

SimWindow::SimWindow(int iWidth, int iHeight) :
    width(iWidth),
    height(iHeight)
{
    createObjects();
}

void SimWindow::resize(int newWidth, int newHeight, bool repos, bool resize)
{
    float xDelta = static_cast<float>(newWidth) / static_cast<float>(width);
    float yDelta = static_cast<float>(newHeight) / static_cast<float>(height);

    width = newWidth;
    height = newHeight;
    std::stringstream ss;
    if (width < 200)
    {
        ss << "Simulator\n[Size:" << width << "x" << height << "]";
    }
    else
    {
        ss << "Simulator [Size:" << width << "x" << height << "]";
    }
    lv_win_set_title(window, ss.str().c_str());
    lv_win_ext_t* ext = static_cast<lv_win_ext_t*>(lv_obj_get_ext_attr(window));
    int headerSize = ext->btn_size;
    lv_obj_set_size(window, width + 8, height + headerSize + 8);
    lv_obj_set_size(drawSurface, width, height);
    if (repos || resize)
    {
        std::vector<lv_obj_t*> children = ObjectTools::GetChildren(drawSurface);
        for (std::vector<lv_obj_t*>::iterator it = children.begin(); it != children.end(); ++it)
        {
            if (repos)
            {
                lv_obj_set_pos(*it,
                               round((lv_obj_get_x(*it) * xDelta)),
                               round((lv_obj_get_y(*it) * yDelta))
                );
            }
            if (resize)
            {
                lv_obj_set_size(*it,
                                round((lv_obj_get_x(*it) * xDelta)),
                                round((lv_obj_get_y(*it) * yDelta))
                );
            }
        }
    }
}

void SimWindow::createObjects()
{
    window = lv_win_create(lv_scr_act(), nullptr);
    std::stringstream ss;
    ss << "Simulator [Size:" << width << "x" << height << "]";
    lv_win_set_title(window, ss.str().c_str());
    lv_win_ext_t* ext = static_cast<lv_win_ext_t*>(lv_obj_get_ext_attr(window));

    int headerSize = ext->btn_size;

    lv_obj_set_size(window, width + 12, height + headerSize + 12);
    int x = (lv_scr_act()->coords.x2 / 2) - (width / 2);
    int y = (lv_scr_act()->coords.y2 / 2) - (width / 2) - 50;
    lv_obj_set_pos(window, x, y);
    lv_win_set_sb_mode(window, LV_SB_MODE_OFF);
    lv_obj_t* winBtn = lv_win_add_btn(window, LV_SYMBOL_SETTINGS);
    lv_obj_set_user_data(winBtn, this);
    lv_obj_set_event_cb(winBtn, settings);
    drawSurface = lv_cont_create(window, nullptr);
    lv_obj_set_style(drawSurface, &lv_style_transp);
    lv_cont_set_layout(drawSurface, LV_LAYOUT_OFF);
    lv_obj_set_auto_realign(drawSurface, false);
    lv_obj_set_size(drawSurface, width, height);
    lv_obj_set_x(drawSurface, lv_obj_get_x(drawSurface) - 3);
    lv_obj_set_y(drawSurface, lv_obj_get_y(drawSurface) - 6);
}

void SimWindow::settings(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    SimWindow* sw = static_cast<SimWindow*>(lv_obj_get_user_data(obj));

    if (sw->settingsWindow != nullptr)
    {
        lv_obj_set_hidden(sw->settingsWindow, false);
    }
    sw->settingsWindow = lv_win_create(lv_scr_act(), nullptr);
    lv_obj_set_size(sw->settingsWindow, 400, 450);
    lv_win_set_title(sw->settingsWindow, "TFT Settings");
    int x = (lv_scr_act()->coords.x2 / 2) - 150;
    int y = (lv_scr_act()->coords.y2 / 2) - 150;
    lv_obj_set_pos(sw->settingsWindow, x, y);
    lv_obj_t* close_btn = lv_win_add_btn(sw->settingsWindow, LV_SYMBOL_CLOSE);
    lv_obj_set_user_data(close_btn, sw->settingsWindow);
    lv_obj_set_event_cb(close_btn, closeSettings);

    lv_obj_t* wLabel = lv_label_create(sw->settingsWindow, nullptr);
    lv_obj_set_pos(wLabel, 56, 23);
    lv_label_set_text(wLabel, "TFT Width:");
    lv_obj_t* hLabel = lv_label_create(sw->settingsWindow, nullptr);
    lv_obj_set_pos(hLabel, 50, 73);
    lv_label_set_text(hLabel, "TFT Height:");

    lv_obj_t* wTA = lv_ta_create(sw->settingsWindow, nullptr);
    lv_ta_set_accepted_chars(wTA, "0123456789");
    lv_obj_set_pos(wTA, 150, 12);
    std::stringstream widthSS;
    lv_obj_set_width(wTA, 75);
    widthSS << sw->width;
    lv_ta_set_text(wTA, widthSS.str().c_str());
    lv_ta_set_one_line(wTA, true);

    lv_obj_t* hTA = lv_ta_create(sw->settingsWindow, nullptr);
    lv_ta_set_accepted_chars(wTA, "0123456789");
    lv_obj_set_pos(hTA, 150, 63);
    std::stringstream heightSS;
    heightSS << sw->height;
    lv_obj_set_width(hTA, 75);
    lv_ta_set_text(hTA, heightSS.str().c_str());
    lv_ta_set_one_line(hTA, true);

    lv_ta_set_cursor_type(wTA, LV_CURSOR_NONE);
    lv_ta_set_cursor_type(hTA, LV_CURSOR_NONE);

    lv_obj_t* kb = lv_kb_create(sw->settingsWindow, nullptr);
    lv_obj_set_y(kb, lv_obj_get_y(kb) - 10);
    static const char* kb_map_num[] =
    {
        "7", "8", "9", "\n",
        "4", "5", "6", "\n",
        "1", "2", "3", "\n",
        LV_SYMBOL_LEFT, "0",LV_SYMBOL_RIGHT, "\n",
        "Bksp", ""
    };
    static const lv_btnm_ctrl_t kb_ctrl_num[] =
    {
        1, 1, 1,
        1, 1, 1,
        1, 1, 1,
        LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG | 1, 1, LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG | 1,
        LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG | 2
    };
    lv_kb_set_map(kb, kb_map_num);
    lv_kb_set_ctrl_map(kb, kb_ctrl_num);

    lv_obj_set_hidden(kb, true);

    lv_obj_t* pos = lv_cb_create(sw->settingsWindow, nullptr);
    lv_cb_set_text(pos, "Reposition objects");
    lv_cb_set_checked(pos, true);
    lv_obj_set_pos(pos, 20, 110);

    lv_obj_t* sze = lv_cb_create(sw->settingsWindow, nullptr);
    lv_cb_set_text(sze, "Resize objects");
    lv_cb_set_checked(sze, true);
    lv_obj_set_pos(sze, 240, 110);

    lv_obj_t* submitBtn = lv_btn_create(sw->settingsWindow, nullptr);
    lv_obj_t* btnLabel = lv_label_create(submitBtn, nullptr);
    lv_label_set_text(btnLabel, "Submit Changes");
    lv_obj_set_pos(submitBtn, 25, 150);
    lv_obj_set_size(submitBtn, 350, 30);
    lv_obj_set_hidden(submitBtn, true);

    cbStruct* cb = new cbStruct();
    cb->sw = sw;
    cb->height = hTA;
    cb->width = wTA;
    cb->sze = sze;
    cb->pos = pos;
    cb->setBtn = submitBtn;
    cb->kb = kb;
    cb->pos = lv_cb_is_checked(pos);
    cb->sze = lv_cb_is_checked(sze);

    lv_obj_set_user_data(wTA, cb);
    lv_obj_set_user_data(hTA, cb);
    lv_obj_set_user_data(submitBtn, cb);
    lv_obj_set_event_cb(submitBtn, submitClicked);

    lv_obj_set_event_cb(wTA, widthClicked);
    lv_obj_set_event_cb(hTA, heightClicked);
}

void SimWindow::closeSettings(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    lv_obj_t* win = static_cast<lv_obj_t*>(lv_obj_get_user_data(obj));
    lv_obj_set_hidden(win, true);
}

void SimWindow::widthClicked(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    cbStruct* cb = static_cast<cbStruct*>(lv_obj_get_user_data(obj));
    int width = atoi(lv_ta_get_text(cb->width));
    int height = atoi(lv_ta_get_text(cb->height));
    if ((width != cb->sw->width || height != cb->sw->height) &&
        (width != 0 && height != 0))
    {
        lv_obj_set_hidden(cb->setBtn, false);
    }
    else
    {
        lv_obj_set_hidden(cb->setBtn, true);
    }
    if (event != LV_EVENT_CLICKED)
        return;
    lv_obj_set_hidden(cb->kb, false);
    lv_ta_set_cursor_type(cb->width, LV_CURSOR_LINE);
    lv_ta_set_cursor_type(cb->height, LV_CURSOR_NONE);
    lv_kb_set_ta(cb->kb, cb->width);
}

void SimWindow::heightClicked(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    cbStruct* cb = static_cast<cbStruct*>(lv_obj_get_user_data(obj));
    int width = atoi(lv_ta_get_text(cb->width));
    int height = atoi(lv_ta_get_text(cb->height));
    if ((width != cb->sw->width || height != cb->sw->height) &&
        (width != 0 && height != 0))
    {
        lv_obj_set_hidden(cb->setBtn, false);
    }
    else
    {
        lv_obj_set_hidden(cb->setBtn, true);
    }
    if (event != LV_EVENT_CLICKED)
        return;
    lv_obj_set_hidden(cb->kb, false);
    lv_ta_set_cursor_type(cb->height, LV_CURSOR_LINE);
    lv_ta_set_cursor_type(cb->width, LV_CURSOR_NONE);
    lv_kb_set_ta(cb->kb, cb->height);
}

void SimWindow::submitClicked(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    cbStruct* cb = static_cast<cbStruct*>(lv_obj_get_user_data(obj));
    int width = atoi(lv_ta_get_text(cb->width));
    int height = atoi(lv_ta_get_text(cb->height));

    cb->sw->resize(width, height, cb->pos, cb->sze);
    lv_obj_set_hidden(cb->sw->settingsWindow, true);
}
