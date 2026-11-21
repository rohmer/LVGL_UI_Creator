#pragma once
#include <any>
#include "../../Utils/EditorEvents.h"
#include <string>
#include <map>
#include <vector>
#include "lvgl/lvgl.h"
#include "PropertyControls.h"
#include "../PropertyWindow.h"

class BtnProperties
{
    friend class PropertyWindow;
    friend class PropertyControls;

public:
    static void UpdateBtnProperties(PropertyWindow* pw, json j);
    static void CreateBtnProperties(PropertyWindow* pw);

private:
    static lv_obj_t* inkIn, * inkOut, * inkWait, * toggle, * layout, * fitTop, * fitBot, * fitRight, * fitLeft;
    static lv_obj_t* rel, * ina, * pr, * tglPr, * tglRel;
    static lv_obj_t* text;

    struct sFitStruct
    {
        int fitNum;
        PropertyWindow* pw;
    };
    static void layoutCB(lv_obj_t* obj, EditorEventCode event);
    static void fitCB(lv_obj_t* obj, EditorEventCode event);
};
