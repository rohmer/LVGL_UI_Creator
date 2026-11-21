#pragma once
#include <string>
#include "../Utils/EditorEvents.h"
#include <vector>
#include "lvgl.h"
#include "../Utils/ObjectTools.h"

class CollapsableWindowManager;

class CollapsableWindow
{
public:
    CollapsableWindow(lv_obj_t* parent,
                      std::string name,
                      bool isCollapsed,
                      int x,
                      int y,
                      int width,
                      int height);
    CollapsableWindow(lv_obj_t* parent,
                      std::string name,
                      bool isCollapsed,
                      int width,
                      int height);
    int GetCurrentHeight();
    lv_obj_t* GetWindow();

    void DeleteObjects();

    void AddObjectToWindow(lv_obj_t* obj);
    lv_obj_t* GetCollapseButton();
    const std::string GetName();
    void ToggleCollapsed();
    void SetPos(int x, int y);
    void SetVerticalPos(int y);
    void DeleteChildren();
    void UpdateHeight(int height);
    void UpdateWidth(int width);
    void SetVisible(bool visible);
    bool IsVisible() const { return visible; }
    bool IsCollapsed() const {return isCollapsed;}
private:
    lv_obj_t *window, *collapseBtnLbl, *collapseBtn, *winTitle;
    std::string name;
    bool isCollapsed;
    bool visible = true;
    int x, y, width, height;

    struct cbStruct
    {
        CollapsableWindow* cw;
        CollapsableWindowManager* cwm;
    };

    void createObjects(lv_obj_t* parent);
    static void collapseClicked(lv_obj_t* obj, EditorEventCode event);
    std::vector<lv_obj_t*> objects;
};
