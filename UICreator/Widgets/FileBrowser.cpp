#include "FileBrowser.h"

pathType FileBrowser::path;
lv_obj_t *FileBrowser::pathBox, *FileBrowser::selectButton;
ListBox* FileBrowser::listBox;
std::vector<std::string> FileBrowser::ext;

FileBrowser::FileBrowser(std::string startingDir, lv_obj_t* parent, lv_style_t* style) :
    style(style)
{
    path = startingDir;
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
}

FileBrowser::FileBrowser(std::string startingDir, lv_area_t coords, lv_obj_t* parent, lv_style_t* style) :
    style(style)
{
    path = startingDir;
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

FileBrowser::FileBrowser(std::string startingDir, std::vector<std::string> extensions, lv_obj_t* parent,
                         lv_style_t* style) :
    style(style)
{
    ext = extensions;
    path = startingDir;
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

FileBrowser::FileBrowser(std::string startingDir, lv_area_t coords, std::vector<std::string> extensions,
                         lv_obj_t* parent, lv_style_t* style) :
    style(style)
{
    ext = extensions;
    path = startingDir;
    if (parent == nullptr)
        parent = lv_scr_act();
    createObjects(parent, coords);
}

void FileBrowser::createObjects(lv_obj_t* parent, lv_area_t coords)
{
    win = lv_win_create(parent, nullptr);
    lv_obj_set_x(win, coords.x1);
    lv_obj_set_y(win, coords.y1);
    lv_obj_set_width(win, coords.x2 - coords.x1);
    lv_obj_set_height(win, coords.y2 - coords.y1);

    lv_win_set_title(win, "File Browser");
    closeBtn = lv_win_add_btn(win, LV_SYMBOL_CLOSE);
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
    selectButton = lv_btn_create(win, nullptr);
    lv_obj_t* sbLabel = lv_label_create(selectButton, nullptr);
    std::stringstream ss;
    ss << LV_SYMBOL_OK << "      Select File";
    lv_label_set_text(sbLabel, ss.str().c_str());
    lv_obj_set_pos(selectButton, lv_obj_get_width(win) - 275, lv_obj_get_height(win) - 145);
    lv_obj_set_size(selectButton, 250, 75);
    lv_obj_set_hidden(selectButton, true);
    lv_obj_set_user_data(selectButton, this);
    lv_obj_set_event_cb(selectButton, selectCBLocal);

    listBox = new ListBox(10, 50, lv_obj_get_width(win) - 20, lv_obj_get_height(win) - 200,
                          lv_font_get_line_height(&lv_font_roboto_12), win, style);
    listBox->AddSelectCallback(listBoxCB);
    refreshObjects();
}

void FileBrowser::refreshObjects()
{
    lv_ta_set_text(pathBox, path.string().c_str());
    listBox->ClearItems();
    std::vector<dirent> files;
    for (const auto& entry : dirit(path))
    {
        #ifdef _WINDOWS
        if (entry.is_directory())
            files.push_back(entry);
        #else
        if(std::experimental::filesystem::is_directory(entry))
            files.push_back(entry);
        #endif
    }
    for (const auto& entry : dirit(path))
    {
        #ifdef _WINDOWS
        if (!entry.is_directory())
        #else
        if(std::experimental::filesystem::is_directory(entry))
        #endif
        {
            if (ext.empty())
            {
                files.push_back(entry);
            }
            else
            {
                bool add = false;
                for (std::string e : ext)
                    if (entry.path().extension() == e)
                    {
                        add = true;
                        break;
                    }
                if (add)
                    files.push_back(entry);
            }
        }
    }
    for (const auto& element : files)
    {
        std::string icon = LV_SYMBOL_FILE;
        #ifdef _WINDOWS
        if (element.is_directory())
        #else
        if(std::experimental::filesystem::is_directory(element))
        #endif        
            icon = LV_SYMBOL_DIRECTORY;
        pathType p = element.path().filename();
        listBox->AddItem(p.string(), icon);
    }
}

void FileBrowser::listBoxCB(std::string selected)
{
    pathType p = path;
    p /= selected;
    if (is_directory(p))
    {
        path = p;
        refreshObjects();
        lv_obj_set_hidden(selectButton, true);
        return;
    }
    lv_ta_set_text(pathBox, p.string().c_str());
    lv_obj_set_hidden(selectButton, false);
}


void FileBrowser::closeWinCB(lv_obj_t* obj, lv_event_t ev)
{
    if (ev == LV_EVENT_CLICKED)
    {
        FileBrowser* fb = static_cast<FileBrowser*>(lv_obj_get_user_data(obj));
        lv_obj_del_async(fb->win);
        fb->active = false;
    }
}

void FileBrowser::upDirCB(lv_obj_t* obj, lv_event_t ev)
{
    if (ev == LV_EVENT_CLICKED)
    {
        FileBrowser* fb = static_cast<FileBrowser*>(lv_obj_get_user_data(obj));
        fb->path = fb->path.parent_path();
        fb->refreshObjects();
    }
}

pathType FileBrowser::SelectedPath()
{
    return path;
}

void FileBrowser::AddCloseCallback(fb_callback cb)
{
    this->selectCB = cb;
}

void FileBrowser::selectCBLocal(lv_obj_t* obj, lv_event_t ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    FileBrowser* fb = static_cast<FileBrowser*>(lv_obj_get_user_data(obj));
    if (fb->selectCB != nullptr)
        fb->selectCB(path);
    delete(fb->listBox);
    lv_obj_del_async(fb->win);
}
