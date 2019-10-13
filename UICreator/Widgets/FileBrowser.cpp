#include "FileBrowser.h"

FileBrowser::FileBrowser(std::string startingDir, lv_obj_t* parent, lv_style_t* style) :
    path(startingDir),
    style(style)
{
    lv_area_t coords;
    if (parent == nullptr)
        parent = lv_scr_act();
    int width=lv_obj_get_width(parent);
    int height = lv_obj_get_height(parent);
    // Default is 3/5s the screen, centered;
    coords.x1 = width / 5;
    coords.y1 = height / 5;
    coords.x2 = width - (width / 5);
    coords.y2 = height - (height / 5);
    if(startingDir.empty())
    {
        char buff[FILENAME_MAX];
        GetCurrentDir(buff, FILENAME_MAX);
        path = buff;
    }
    createObjects(parent, coords);
}

FileBrowser::FileBrowser(std::string startingDir, lv_area_t coords, lv_obj_t* parent, lv_style_t* style) :
    path(startingDir),
    style(style)
{
    if (parent == nullptr)
        parent = lv_scr_act();
    if (startingDir.empty())
    {
        char buff[FILENAME_MAX];
        GetCurrentDir(buff, FILENAME_MAX);
        path = buff;
    }
    createObjects(parent, coords);
}

FileBrowser::FileBrowser(std::string startingDir, std::vector<std::string> extensions, lv_obj_t* parent, lv_style_t* style) :
    path(startingDir),
    ext(extensions),
    style(style)
{
    lv_area_t coords;
    if (parent == nullptr)
        parent = lv_scr_act();
    int width = lv_obj_get_width(parent);
    int height = lv_obj_get_height(parent);
    // Default is 3/5s the screen, centered;
    coords.x1 = width / 5;
    coords.y1 = height / 5;
    coords.x2 = width - (width / 5);
    coords.y2 = height - (height / 5);
    if (startingDir.empty())
    {
        char buff[FILENAME_MAX];
        GetCurrentDir(buff, FILENAME_MAX);
        path = buff;
    }
    createObjects(parent, coords);
    active = true;
}

FileBrowser::FileBrowser(std::string startingDir, lv_area_t coords, std::vector<std::string> extensions, lv_obj_t* parent, lv_style_t *style) :
    path(startingDir),
    ext(extensions),
    style(style)
{
    if (parent == nullptr)
        parent = lv_scr_act();
    createObjects(parent, coords);
}

bool FileBrowser::IsActive()
{
    return active;
}

void FileBrowser::createObjects(lv_obj_t* parent, lv_area_t coords)
{
    win = lv_win_create(parent, nullptr);
    lv_obj_set_x(win, coords.x1);
    lv_obj_set_y(win, coords.y1);
    lv_obj_set_width(win, coords.x2 - coords.x1);
    lv_obj_set_height(win, coords.y2 - coords.y1);

    lv_win_set_title(win, "File Browser");
    closeBtn=lv_win_add_btn(win, LV_SYMBOL_CLOSE);
    lv_obj_set_user_data(closeBtn, this);
    lv_obj_set_event_cb(closeBtn, closeWinCB);
    lv_win_set_style(win, LV_WIN_STYLE_BG, style);
    lv_win_set_style(win, LV_WIN_STYLE_CONTENT, style);
    lv_win_set_style(win, LV_WIN_STYLE_HEADER, style);

    // Path Bar
    upDir = lv_btn_create(win, nullptr);
    lv_obj_t* udLab = lv_label_create(upDir, nullptr);    
    lv_label_set_text(udLab, LV_SYMBOL_UP);
    lv_label_set_style(udLab, LV_LABEL_STYLE_MAIN, style);
    lv_obj_set_x(upDir, 10);
    lv_obj_set_y(upDir, 10);
    lv_obj_set_width(upDir, 30);
    lv_obj_set_height(upDir, 30);
    lv_obj_set_user_data(upDir, this);
    lv_obj_set_event_cb(upDir, upDirCB);
    lv_btn_set_style(upDir, LV_BTN_STYLE_INA, style);

    pathBox = lv_ta_create(win, nullptr);
    lv_obj_set_x(pathBox, 45);
    lv_obj_set_y(pathBox, 5);
    lv_obj_set_width(pathBox, lv_obj_get_width(win) - 100);
    lv_obj_set_height(pathBox, 40);    
    lv_obj_set_user_data(closeBtn, this);
    lv_ta_set_style(pathBox, LV_TA_STYLE_BG, style);

    refreshBtn = lv_btn_create(win, nullptr);
    lv_obj_t* rbLab = lv_label_create(refreshBtn, nullptr);
    lv_label_set_text(rbLab, LV_SYMBOL_REFRESH);
    lv_obj_set_x(refreshBtn, lv_obj_get_width(win) - 50);
    lv_obj_set_y(refreshBtn, 10);
    lv_obj_set_height(refreshBtn, 30);
    lv_obj_set_width(refreshBtn, 30);
    lv_obj_set_user_data(refreshBtn, this);
    lv_obj_set_event_cb(refreshBtn, refreshCB);
    lv_btn_set_style(refreshBtn, LV_BTN_STYLE_INA, style);

    listBox = new ListBox(10, 50, lv_obj_get_width(win) - 20, lv_obj_get_height(win) - 200, lv_font_get_line_height(&lv_font_roboto_12), win, style);
    refreshObjects();
}

void FileBrowser::refreshObjects()
{
    lv_ta_set_text(pathBox, path.string().c_str());
    listBox->ClearItems();
    // TODO: Put the directories first
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        std::string icon = LV_SYMBOL_FILE;
        if (entry.is_directory())
            icon = LV_SYMBOL_DIRECTORY;
        std::filesystem::path p = entry.path().filename();
        listBox->AddItem(p.string(), icon);
    }
}

void FileBrowser::closeWinCB(lv_obj_t* obj, lv_event_t ev)
{
    if(ev==LV_EVENT_CLICKED)
    {
        FileBrowser *fb = (FileBrowser*)lv_obj_get_user_data(obj);
        lv_obj_del_async(fb->win);
        fb->active = false;
    }
}

void FileBrowser::upDirCB(lv_obj_t* obj, lv_event_t ev)
{
    if (ev == LV_EVENT_CLICKED)
    {
        FileBrowser* fb = (FileBrowser*)lv_obj_get_user_data(obj);
        fb->path = fb->path.parent_path();
        fb->refreshObjects();
    }
}

std::filesystem::path FileBrowser::SelectedPath()
{
    return path;
}

void FileBrowser::refreshCB(lv_obj_t* obj, lv_event_t ev)
{

}
