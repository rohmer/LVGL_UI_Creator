#pragma once
#include <string>
#include "../Utils/EditorEvents.h"
#include <vector>
#include <map>
#include <functional>
#include "lvgl/lvgl.h"
#include "../Widgets/TreeView.h"
#include "../Widgets/MinimizableWindow.h"
#include <Serialization/ObjectUserData.h>
#include <Serialization/ObjectSerializer.h>

#pragma region Object Creation Includes
#include "../ObjectCreation/Arc.h"
#include "../ObjectCreation/Bar.h"
#include "../ObjectCreation/Button.h"
#include "../ObjectCreation/ButtonMatrix.h"
#include "../ObjectCreation/Calendar.h"
#include "../ObjectCreation/Canvas.h"
#pragma endregion
#undef GetObject

class PropertyWindow;
class ToolBar;

class ToolTray
{
public:
    std::function<void(lv_obj_t*)> onWidgetCreated;
    std::function<void()> onWidgetChanged;
    ToolTray(lv_obj_t* parent,
             TreeView* objectTree,
             lv_obj_t* drawSurface,
             PropertyWindow* propWin,
             ToolBar* toolBar);
    lv_obj_t* GetWindow() const { return toolWin; }
    void AdoptWidget(lv_obj_t* object, unsigned int parentID);
    lv_obj_t* CreateBootstrapPreset(int preset, bool dark = false);
    lv_obj_t* CreateBootstrapTemplate(const std::string& id);
    PropertyWindow* GetPropertyWindow() { return propertyWindow; }
    void RefreshKeyboardBindings();

private:
    lv_obj_t* toolWin;
    ToolBar* toolBar;
    MinimizableWindow* minWin;
    TreeView* objTree;
    lv_obj_t* drawSurface;
    PropertyWindow* propertyWindow;
    void initializeToolMatrix();
    void initializeWidgetButtons(lv_obj_t* parent, int category);
    void initializeBootstrapButtons(lv_obj_t* parent, bool templates);
    void BindWidget(lv_obj_t* object, bool ownsUserData);
#if LVGL_VERSION_MAJOR >= 9
    static void BindContent(lv_obj_t* object);
#endif
    uint16_t currentID = 0;

    std::vector<lv_obj_t*> widgetButtons;
    lv_obj_t* lastWidget = nullptr;
    std::map<lv_obj_t*,std::pair<int,lv_obj_t*>> keyboardTargets;

    struct sCB
    {
        ToolTray* toolTray;
        int typeID;
    };

#pragma region Callbacks
    static void create_obj_cb(lv_obj_t* obj, EditorEventCode ev);
    static void updateProperties(lv_obj_t* obj, EditorEventCode ev);
#pragma endregion
};
