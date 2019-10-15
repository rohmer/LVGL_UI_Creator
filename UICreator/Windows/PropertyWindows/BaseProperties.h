#pragma once
#include <any>
#include <string>
#include <map>
#include <vector>
#include "lvgl/lvgl.h"
#include "PropertyControls.h"
#include "../PropertyWindow.h"

class BaseProperties
{
public:
    static void CreateBaseObjProps(PropertyWindow* pw);
    static void UpdateGlobalProps(PropertyWindow* pw, json j);

private:
#pragma region Global Property Inputs
    // Position/Area
    static lv_obj_t *taX, *taY, *taWidth, *taHeight;
    static lv_obj_t* styleDD;
    static lv_obj_t *hidden, *click, *top, *parentEvent, *opaScaleEnable, *opaScale;
    static lv_obj_t *drag, *dragDir, *dragThrow, *dragParent;
    static lv_obj_t *protNone, *protPos, *protFollow, *protParent, *protPressLost, *protClickFocus, *protChildChg;
#pragma endregion

    static CollapsableWindow* baseObjProps;
};
