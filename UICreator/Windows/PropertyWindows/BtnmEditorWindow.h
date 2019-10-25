#pragma once
#include <any>
#include <string>
#include <map>
#include <vector>
#include "lvgl/lvgl.h"
#include "../PropertyWindow.h"

class BtnmEditorWindow
{
public:
    static void CreateBtnmEditorWindow(PropertyWindow* pw);
    struct btnStruct
    {
        std::string btnName;
        int btnNum, lineNum;
        std::string tn, n;
    };

private:
    static lv_obj_t* window, * preview, *previewCont, *mapTA;
    static std::string btnMap;

    static int listContWidth, listContHeight;
    static PropertyWindow* pw;
    static CollapsableWindowManager* cwm;
    static CollapsableWindow* defWin;

    static void closeBtnCB(lv_obj_t* obj, lv_event_t event);
    static void assignMap(lv_obj_t* obj, lv_event_t event);

    static void createEditorLines();

    static void resize();
    static void updateMap();
    
    static std::vector<btnStruct> buttons;

    
};