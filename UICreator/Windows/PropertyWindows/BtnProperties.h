#pragma once
#include <any>
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
};
