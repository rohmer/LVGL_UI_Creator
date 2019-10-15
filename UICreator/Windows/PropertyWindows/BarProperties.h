#pragma once
#include <any>
#include <string>
#include <map>
#include <vector>
#include "lvgl/lvgl.h"
#include "PropertyControls.h"
#include "../PropertyWindow.h"

class BarProperties
{
    friend class PropertyWindow;
    friend class PropertyControls;

public:
    static void UpdateBarProperties(PropertyWindow* pw, json j);
    static void CreateBarProperties(PropertyWindow* pw);

    static lv_obj_t* rngStart, * rngEnd, * sym, * animTime;
};