#pragma once
#include <map>
#include <functional>
#include <memory>
#include "../Utils/EditorEvents.h"
#include <string>
#include <lvgl/lvgl.h>
#include "SimWindow.h"
#include "PropertyWindow.h"
#include "../Widgets/MinimizableWindow.h"
#include "../Variable/Variable.h"
#include "../Utils/EditorObjectRef.h"

class ToolBar
{
public:
    ToolBar(PropertyWindow* propertyWindow,
            SimWindow* simWindow);
    lv_obj_t* GetWindow() const { return window->GetWindowObj(); }
    lv_obj_t* GetAssetTab() const { return contextTab; }
    lv_obj_t* GetTabView() const {return tabView;}
    lv_obj_t* GetVariableTab() const { return variableTab; }
    void SetContextActive();
    void SetSelectedObject();
    json VariablesToJSON() const;
    void LoadVariables(const json& records);
    void RefreshVariableContext(const json& document, bool resetTargets = false);
    void CommitVariableTargets();
    bool VariableDialogOpen() const { return bindingDialog != nullptr; }
    void CancelVariableConnection() { CloseVariableBinding(); }
    std::function<json()> variableDocument;
    std::function<size_t()> variableScreen;
    std::function<void(size_t, int)> revealVariableTarget;
    std::function<void(const std::string&, const std::string&)> variableRenamed;
    std::function<void()> variableSelected;

private:
    PropertyWindow* propertyWindow;
    SimWindow* simWindow;
    MinimizableWindow* window;
    lv_obj_t *tabView, *contextTab, *variableTab;
    lv_obj_t* selectedObject;

    // Variable Tab Objects
    lv_obj_t *variableList, *addVarBtn, *removeVarBtn, *vPropCont;
    lv_obj_t *varNameTA, *varHistTA, *varDefaultValTA, *varDTDropDown;
    lv_obj_t *varCodeTA = nullptr, *varCodeType = nullptr, *varStatus = nullptr;
    lv_obj_t *varCodePreview = nullptr, *varBindingList = nullptr, *varBindingSummary = nullptr;
    lv_obj_t *addBindingBtn = nullptr, *selectedBindingBtn = nullptr;
    lv_obj_t *bindingDialog = nullptr, *bindingScreen = nullptr, *bindingWidget = nullptr;
    lv_obj_t *bindingProperty = nullptr, *bindingDirection = nullptr, *bindingStatus = nullptr;
    json variableContext = json::object();
    std::map<std::pair<size_t, size_t>, std::shared_ptr<EditorObjectRef>> variableTargets;
    std::vector<int> bindingWidgetIds;
    std::vector<Serialization::VariableBindingProperty> bindingProperties;
    int editingBinding = -1;
    bool loadingBinding = false;

    template <typename T>
    static std::map<uint16_t, Variable<T>> oidToVariable;
    template <typename T>
    static std::map<uint16_t, lv_obj_t*> oidToObj;
    void createObjects();
    void createObjectTools();
    void createVariableTools();

    // Callbacks

    static void addVarCallback(lv_obj_t* obj, EditorEventCode event);
    static void removeVarCallback(lv_obj_t* obj, EditorEventCode event);
    static void updateVariable(lv_obj_t* obj, EditorEventCode event);
    static void selectProperty(lv_obj_t* obj, EditorEventCode event);
    void RefreshVariables();
    void SelectVariable(int index);
    void RefreshVariableCodeTypes();
    void RefreshVariableCode();
    void RefreshVariableBindings();
    void TrackVariableTargets();
    void ShowVariableBinding(int index = -1, bool useSelected = false);
    void RefreshBindingWidgets(int selectedId = -1);
    void RefreshBindingProperties(const std::string& path = "");
    void SaveVariableBinding();
    void CloseVariableBinding();
    void VariableMessage(const std::string& message, bool error = false);
    json variables = json::array();
    int selectedVariable = -1;
    bool loadingVariable = false;

    template <typename T>
    static Variable<T>* currentVar;
};
