#pragma once
#include <any>
#include <string>
#include <map>
#include <vector>
#include "lvgl/lvgl.h"
#include "PropertyControls.h"
#include "../PropertyWindow.h"

class ArcProperties
{
    friend class PropertyWindow;
    friend class PropertyControls;

public:
    static void UpdateArcProperties(PropertyWindow *pw, json j);
    static void CreateArcProperties(PropertyWindow *pw);

 private:
#pragma region Arc Properties
    static lv_obj_t* arcStartTA, * arcEndTA, * arcLineWidth, * arcLineRound;
#pragma endregion
};