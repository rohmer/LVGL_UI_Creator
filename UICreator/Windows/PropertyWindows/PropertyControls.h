#pragma once
#include <map>
#include <string>
#include <vector>
#include "lvgl/lvgl.h"
#include "../PropertyWindow.h"

struct sPropChange
{
    PropertyWindow* pw;
    std::string propertyPath;
    json objectJSON;
};

class PropertyControls
{
public:
    static lv_obj_t* createNumericEntry(PropertyWindow* pw, lv_obj_t* parent, std::string labelTxt,
                                        std::string propertyPath);
    static lv_obj_t* createCBEntry(PropertyWindow* pw, lv_obj_t* parent, std::string labelTxt,
                                   std::string propertyPath);
    static lv_obj_t* createStyleEntry(PropertyWindow* pw, lv_obj_t* parent, std::string labelTxt,
                                      std::string propertyPath);

    static void numericEntryCB(lv_obj_t* obj, lv_event_t event);
    static void ddListCB(lv_obj_t* obj, lv_event_t event);
    static void textBoxCB(lv_obj_t* obj, lv_event_t event);
    static void checkBoxCB(lv_obj_t* obj, lv_event_t event);
    static void createStyleCB(lv_obj_t* obj, lv_event_t event);
    static void assignColor(lv_color_t color, std::any objData);
    static void styleListCB(lv_obj_t* obj, lv_event_t event);
};
