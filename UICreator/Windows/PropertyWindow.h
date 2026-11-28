#pragma once
#include <any>
#include <algorithm>
#include "../Utils/EditorEvents.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
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
#include "PropertyWindows/BtnmProperties.h"
#include "../../3rdParty/JSON/json.hpp"
#include <Serialization/ObjectSerializer.h>
#include <Serialization/Style.h>
#include <spdlog/spdlog.h>
#include "../Fonts/FontLibrary.h"
#include "../Images/ImageLibrary.h"
#include "../Widgets/TrieLibrary.h"

using json = nlohmann::json;

class PropertyWindow
{
public:
    PropertyWindow(lv_indev_t* kb_indev, SimWindow* simWindow, int screenWidth, int screenHeight);
    ~PropertyWindow();
    TreeView* GetObjectTree();
    void SetSelectedObject(lv_obj_t* object, bool synchronizeTree = true, bool preserveSelection = false);
    void RefreshSelectedObjectAsync(lv_obj_t* object);
    void ShowProjectProperties();
    bool ShowingProjectProperties() const { return projectPropertiesVisible; }
    void RefreshProjectPropertiesAsync();
    std::function<json()> projectKeyboardSettings;
    std::function<void(const json&)> applyProjectKeyboardSettings;
    void SetSelection(const std::vector<lv_obj_t*>& objects);
    std::function<void()> onSelectionChanged;
    const std::vector<lv_obj_t*>& SelectedObjects() const { return selectedObjects; }
    std::vector<lv_obj_t*> SelectionRoots() const;
    void AddStyle(json styleJson);
    lv_obj_t* GetSelectedObject();
    ImageLibrary& Images() { return images; }
    FontLibrary& Fonts() { return fonts; }
    std::function<size_t(const std::string&)> projectFontUsage;
    std::function<void()> prepareProjectFontChange;
    size_t FontUsage(const std::string& id) { return projectFontUsage?projectFontUsage(id):fonts.Usage(ProjectScreen(),id); }
    TrieLibrary& Tries() { return tries; }
    lv_obj_t* ProjectScreen() { return simWindow->GetDrawSurface(); }
    void AddFontSelector();
    void DeleteSelectedObject();
    bool RenameSelected(const std::string& name);
    void ForgetObject(lv_obj_t* object) {
        selectedObjects.erase(std::remove(selectedObjects.begin(), selectedObjects.end(), object), selectedObjects.end());
        if(selectedObject == object)selectedObject = nullptr;
    }

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
    struct RefreshState { PropertyWindow* window; size_t generation = 0; };
    std::shared_ptr<RefreshState> refreshState;
    bool projectPropertiesVisible = false;
    std::vector<std::pair<CollapsableWindow*, bool>> projectHiddenPanels;
    FontLibrary fonts;
    ImageLibrary images;
    TrieLibrary tries;
    lv_obj_t* fontSelectorRow = nullptr;

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

    std::vector<lv_obj_t*> selectedObjects;
    lv_obj_t* selectedObject = nullptr;

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
    static void theme_select_event_handler(lv_obj_t* roller, EditorEventCode event);
    static void hue_select_event_cb(lv_obj_t* roller, EditorEventCode event);
    static bool drawing;

    static lv_group_t* kbGroup;
    lv_indev_t* kbIndev;
};

