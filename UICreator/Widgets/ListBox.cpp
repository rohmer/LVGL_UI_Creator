#include "ListBox.h"
ListBox::lb_callback ListBox::selectCallback = nullptr;
ListBox::sListItem* ListBox::selected = nullptr;

ListBox::ListBox(
    unsigned int x,
    unsigned int y,
    unsigned int width,
    unsigned int height,
    unsigned int lineHeight,
    lv_obj_t* parent,
    lv_style_t* style
) :
    lineHeight(lineHeight)
{
    styles[MAIN] = style;
    if (parent == nullptr)
        parent = lv_scr_act();
    cont = lv_page_create(parent, nullptr);
    lv_page_set_style(cont, LV_PAGE_STYLE_BG, styles[MAIN]);
    lv_obj_set_x(cont, x);
    lv_obj_set_y(cont, y);
    lv_obj_set_width(cont, width);
    lv_obj_set_height(cont, height);
}

ListBox::~ListBox()
{
    ClearItems();
}

void ListBox::ClearItems()
{
    for (sListItem* element : items)
    {
        if (element->icon != nullptr)
            lv_obj_del(element->icon);
        if (element->txt != nullptr)
            lv_obj_del(element->txt);
        if (element->selectBox != nullptr)
            lv_obj_del(element->selectBox);
        delete(element);
    }
    selected = nullptr;
    items.clear();
}

void ListBox::AddStyle(eListBoxStyles styleType, lv_style_t* style)
{
    styles[styleType] = style;
}

void ListBox::AddItem(std::string name, std::string icon)
{
    if (styles.find(LIST_ICON) == styles.end())
        styles[LIST_ICON] = styles[MAIN];
    if (styles.find(LIST_ITEM) == styles.end())
        styles[LIST_ITEM] = styles[MAIN];

    sListItem* li = new sListItem();
    int y = 3 + (items.size() * lineHeight + 3);
    li->selectBox = lv_obj_create(cont, nullptr);
    lv_obj_set_style(li->selectBox, &lv_style_transp);
    lv_obj_set_event_cb(li->selectBox, selectCBLocal);
    lv_obj_set_pos(li->selectBox, 5, y);
    lv_obj_set_width(li->selectBox, lv_obj_get_width(cont) - 20);
    lv_obj_set_height(li->selectBox, lineHeight);

    if (!icon.empty())
    {
        li->icon = lv_label_create(cont, nullptr);
        lv_label_set_style(li->icon, LV_LABEL_STYLE_MAIN, styles[LIST_ICON]);
        lv_label_set_text(li->icon, icon.c_str());
        lv_obj_set_height(li->icon, lineHeight);
        lv_obj_set_width(li->icon, lineHeight);
        lv_obj_set_pos(li->icon, 6, y);
    }
    li->txt = lv_label_create(cont, nullptr);
    lv_label_set_style(li->icon, LV_LABEL_STYLE_MAIN, styles[LIST_ITEM]);
    lv_label_set_text(li->txt, name.c_str());
    lv_obj_set_pos(li->txt, 10 + lineHeight, y);
    lv_obj_set_user_data(li->selectBox, li);
    li->id = items.size() + 1;
    items.push_back(li);
}

void ListBox::selectCBLocal(lv_obj_t* obj, lv_event_t ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    if (selectCallback == nullptr)
        return;
    sListItem* li = static_cast<sListItem*>(lv_obj_get_user_data(obj));
    std::string val(lv_label_get_text(li->txt));
    lv_obj_set_style(li->selectBox, &lv_style_plain_color);
    if (selected != nullptr && selected->id == li->id)
    {
        lv_obj_set_style(li->selectBox, &lv_style_transp);
        selected = nullptr;
    }
    else
    {
        if (selected != nullptr)
        {
            lv_obj_set_style(selected->selectBox, &lv_style_transp);
        }
        selected = li;
        lv_obj_set_style(selected->selectBox, &lv_style_plain_color);
    }
    selectCallback(val);
}

void ListBox::AddSelectCallback(lb_callback cb)
{
    selectCallback = cb;
}
