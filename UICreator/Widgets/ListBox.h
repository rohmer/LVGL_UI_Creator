#pragma once
#include <any>
#include <map>
#include <string>
#include <vector>
#include "lvgl.h"

class ListBox
{
public:
    typedef void (*lb_callback)(std::string selectedItem);

    enum eListBoxStyles
    {
        MAIN,
        LIST_ITEM,
        LIST_ICON
    };

    ~ListBox();

    ListBox(
        unsigned int x,
        unsigned int y,
        unsigned int width,
        unsigned int height,
        unsigned int lineHeight,
        lv_obj_t* parent = nullptr,
        lv_style_t* style = &lv_style_pretty_color);

    void ClearItems();
    void AddItem(std::string name, std::string icon = "");
    void AddStyle(eListBoxStyles, lv_style_t* style);
    void AddSelectCallback(lb_callback cb);

    struct sListItem
    {
        int id = -1;
        lv_obj_t* icon = nullptr;
        lv_obj_t* txt = nullptr;
        lv_obj_t* selectBox = nullptr;
    };

private:
    lv_obj_t* cont;

    bool hasIcon = false;
    int lineHeight;

    std::map<eListBoxStyles, lv_style_t*> styles;
    std::vector<sListItem*> items;
    static lb_callback selectCallback;

    static void selectCBLocal(lv_obj_t* obj, lv_event_t ev);
    static sListItem* selected;
};
