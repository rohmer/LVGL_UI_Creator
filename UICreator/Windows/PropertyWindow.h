#pragma once
#include <any>
#include <string>
#include <map>
#include <vector>
#include "lvgl/lvgl.h"
#include "ToolTray.h"
#include "SimWindow.h"
#include "../Widgets/CollapsableWindowManager.h"
#include "../Widgets/CollapsableWindow.h"
#include "../Widgets/MinimizableWindow.h"
#include "../Widgets/TreeView.h"
#include "../Widgets/ColorPicker.h"
#include "UIObjectData.h"
#include "PropertyWindows/PropertyControls.h"
#include "PropertyWindows/BaseProperties.h"
#include "PropertyWindows/ArcProperties.h"
#include "PropertyWindows/BarProperties.h"
#include "PropertyWindows/BtnProperties.h"
#include "../JSON/json.hpp"
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Style.h>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

class PropertyWindow
{
public:
    PropertyWindow(lv_indev_t* kb_indev, SimWindow* simWindow, int screenWidth, int screenHeight);
    TreeView* GetObjectTree();
    void SetSelectedObject(lv_obj_t* object);
    void AddStyle(json styleJson);
    lv_obj_t* GetSelectedObject();

    lv_group_t* GetKBGroup()
    {
        return kbGroup;
    }

    lv_indev_t* GetKBInDev()
    {
        return kbIndev;
    }

    bool Drawing()
    {
        return drawing;
    }

    void Drawing(bool val)
    {
        drawing = val;
    }

    CollapsableWindowManager* GetCWM()
    {
        return cwm;
    }

    CollapsableWindow* ObjectPropWin()
    {
        return objProps;
    }

    lv_obj_t* GetWindow()
    {
        return propertyWin;
    }

    struct sOData
    {
        PropertyWindow* pw;
        std::string objName;
    };

    struct sPropChange
    {
        PropertyWindow* pw;
        std::string propertyPath;
    };

    enum eObjType
    {
        ARC,
        BAR,
        BUTTON,
        BUTTONMATRIX,
        CALENDAR,
        CANVAS,
        CHECKBOX,
        CHART,
        CONTAINER,
        DROPDOWN,
        GAUGE,
        IMAGE,
        IMAGEBUTTON,
        KEYBOARD,
        LABEL,
        LED,
        LINE,
        LIST,
        LINEMETER,
        MESSAGEBOX,
        PAGE,
        PRELOADER,
        ROLLER,
        SLIDER,
        SPINBOX,
        SWITCH,
        TABLE,
        TABVIEW,
        TEXTAREA,
        TILEVIEW,
        WINDOW,
        NONE
    };

    eObjType CurrentlyLoadedProp = NONE;
    std::map<std::string, json> Styles;
    std::map<std::string, lv_style_t*> StylePtrs;

private:

    MinimizableWindow* minWin;
    SimWindow* simWindow;
    lv_obj_t* propertyWin;
    int screenX, screenY;
    bool globalExpanded;
    CollapsableWindow *globalProps, *baseObjProps, *treeWin, *objProps;
    CollapsableWindowManager* cwm;
    TreeView* treeView;
    static lv_theme_t* activeTheme;
    static std::vector<lv_theme_t *> themes;

    lv_obj_t* selectedObject;

    struct sInp
    {
        std::string name;
        PropertyWindow* pw;
    };

    static void initializeThemes(uint16_t hue);
    void createPropertyWin();
    void createGlobalProps();
    void createObjProps();
    void createTreeView();
    void initStyles();

    static void objSelectCB(TreeNode* node);
    static void deleteCB(TreeNode* node);
    static void theme_select_event_handler(lv_obj_t* roller, lv_event_t event);
    static void hue_select_event_cb(lv_obj_t* roller, lv_event_t event);
    static bool drawing;

    static lv_group_t* kbGroup;
    lv_indev_t* kbIndev;
};
