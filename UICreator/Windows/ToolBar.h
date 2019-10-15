#pragma once
#include <map>
#include <string>
#include <lvgl/lvgl.h>
#include "SimWindow.h"
#include "PropertyWindow.h"
#include "../Widgets/MinimizableWindow.h"
#include "../Variable/Variable.h"

class ToolBar
{
public:
    ToolBar(PropertyWindow* propertyWindow,
            SimWindow* simWindow);
    void SetContextActive();
    void SetSelectedObject();

private:
    PropertyWindow* propertyWindow;
    SimWindow* simWindow;
    MinimizableWindow* window;
    lv_obj_t *tabView, *contextTab, *variableTab;
    lv_obj_t* selectedObject;

    // Variable Tab Objects
    lv_obj_t *variableList, *addVarBtn, *removeVarBtn, *vPropCont;
    lv_obj_t *varNameTA, *varHistTA, *varDefaultValTA, *varDTDropDown;

    template <typename T>
    static std::map<uint16_t, Variable<T>> oidToVariable;
    template <typename T>
    static std::map<uint16_t, lv_obj_t*> oidToObj;
    void createObjects();
    void createObjectTools();
    void createVariableTools();

    // Callbacks

    static void addVarCallback(lv_obj_t* obj, lv_event_t event);
    static void removeVarCallback(lv_obj_t* obj, lv_event_t event);
    static void updateVariable(lv_obj_t* obj, lv_event_t event);
    static void selectProperty(lv_obj_t* obj, lv_event_t event);

    template <typename T>
    static Variable<T>* currentVar;
};
