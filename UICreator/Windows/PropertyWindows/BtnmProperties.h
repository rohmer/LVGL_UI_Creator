#pragma once
#include <any>
#include <string>
#include <map>
#include <vector>
#include "lvgl/lvgl.h"
#include "PropertyControls.h"
#include "../PropertyWindow.h"
#include "BtnmEditorWindow.h"

class BtnmProperties
{
    friend class PropertyWindow;
    friend class PropertyControls;

public:
    static void UpdateBtnmProperties(PropertyWindow* pw, json j);
    static void CreateBtnmProperties(PropertyWindow* pw);

private:
    static lv_obj_t* buttonEditor;
    static void buttonEditorCB(lv_obj_t* obj, lv_event_t event);

};