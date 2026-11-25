#include "PropertyWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorDrag.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorEvents.h"
#include "../Utils/EditorStyle.h"
#include "ToolBar.h"
#include "ToolTray.h"
#include "Themes.h"
#include "PropertyWindows/BasicProperties.h"
#include "PropertyWindows/AdvancedProperties.h"
#include "PropertyWindows/CustomProperties.h"
#include "PropertyWindows/ImageProperties.h"
#include "../Fonts/BuiltinFonts.h"
#include "../Fonts/FontConverterWindow.h"

lv_theme_t* PropertyWindow::activeTheme;
std::vector<lv_theme_t*> PropertyWindow::themes;
namespace {
void StylePropertyControls(lv_obj_t* parent)
{
    const auto* type=lv_obj_get_class(parent);
    if(type==&lv_textarea_class || type==&lv_dropdown_class) EditorStyle::Control(parent);
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i) StylePropertyControls(lv_obj_get_child(parent,i));
}
}
bool PropertyWindow::drawing = false;
lv_group_t* PropertyWindow::kbGroup;

PropertyWindow::PropertyWindow(lv_indev_t* kb_indev,
                               SimWindow* simWindow,
                               int screenWidth,
                               int screenHeight) :
    kbIndev(kb_indev),
    simWindow(simWindow),
    screenX(screenWidth),
    screenY(screenHeight)
{
    kbGroup = lv_group_create();
    lv_indev_set_group(GetKBInDev(), kbGroup);
    simWindow->SetKeyboardGroup(kbGroup);
    initializeThemes(120);
    initStyles();
    createPropertyWin();
}

TreeView* PropertyWindow::GetObjectTree()
{
    return treeView;
}

lv_obj_t* PropertyWindow::GetSelectedObject()
{
    return selectedObject;
}

bool PropertyWindow::RenameSelected(const std::string& name)
{
    return treeView && selectedObject && treeView->RenameObject(selectedObject, name);
}

void PropertyWindow::createPropertyWin()
{
    minWin = new MinimizableWindow("Properties", screenX - 400, 0, 400, screenY, nullptr, false, screenX / 2 + 40, 10);
    auto* window = minWin->GetWindowObj();
    propertyWin = EditorWindow::Content(window);
#if LVGL_VERSION_MAJOR >= 9
    EditorDrag::Attach(EditorWindow::Header(window), window);
#else
    lv_obj_set_drag(propertyWin, true);
    lv_obj_set_drag_dir(propertyWin, LV_DRAG_DIR_ALL);
    lv_obj_set_protect(propertyWin, LV_PROTECT_NONE);
#endif
    cwm = new CollapsableWindowManager(propertyWin, 10, 5, 10, true);
    createGlobalProps();
#if LVGL_VERSION_MAJOR >= 9
    createObjProps();
    BaseProperties::CreateBaseObjProps(this);
#else
    BaseProperties::CreateBaseObjProps(this);
    createObjProps();
#endif
    // TreeView is the last property
    createTreeView();
    cwm->Update();
}

void PropertyWindow::DeleteSelectedObject()
{
    EditorEdit::Scope edit;
    auto roots = SelectionRoots();
    if(roots.empty() && selectedObject && selectedObject != simWindow->GetDrawSurface())roots.push_back(selectedObject);
    if(roots.empty())return;
    selectedObject = nullptr;selectedObjects.clear();
    for(auto* object : roots)treeView->DeleteObject(object);
    objProps->DeleteChildren();objProps->UpdateHeight(0);CurrentlyLoadedProp = NONE;cwm->Update();
    SetSelectedObject(simWindow->GetDrawSurface());
}

void PropertyWindow::SetSelectedObject(lv_obj_t* object, bool synchronizeTree, bool preserveSelection)
{
    if (!object)
        return;
    if(!preserveSelection) {
        for(auto* previous : selectedObjects)if(previous && lv_obj_is_valid(previous))HighlightEditorObject(previous, false);
        selectedObjects.clear();
        if(object != simWindow->GetDrawSurface())selectedObjects.push_back(object);
    }
    selectedObject = object;
    if(onSelectionChanged)onSelectionChanged();
    objProps->SetVisible(true);
    if (synchronizeTree) treeView->ShowSelection(selectedObjects, object);
#if LVGL_VERSION_MAJOR >= 9
    lv_group_remove_all_objs(kbGroup);
    lv_group_add_obj(kbGroup, object);
    lv_group_focus_obj(object);
    lv_group_set_editing(kbGroup, true);
    if(const auto* kind=Serialization::CustomWidgets::Type(object)) {
        if(std::string(kind)=="textBox")if(auto* entry=Serialization::CustomWidgets::Part(object,"Entry")) {
            lv_group_add_obj(kbGroup,entry);lv_group_focus_obj(entry);
        }
    }
#endif
    json j = Serialization::ObjectSerializer::SerializeObject(object);
    ObjectUserData* objectData = (ObjectUserData*)lv_obj_get_user_data(object);
    if (!objectData)
    {
        Serialization::LVObject::FromJSON(j["base"], object);
        objectData = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
    }
    objectData->objectJson = j;
    lv_obj_set_user_data(object, objectData);
    BaseProperties::UpdateGlobalProps(this, j);
#if LVGL_VERSION_MAJOR >= 9
    if (CustomProperties::Supports(object) || AdvancedProperties::Supports(object))
    {
        if (CustomProperties::Supports(object)) CustomProperties::Update(this, j);
        else AdvancedProperties::Update(this, j);
        AddFontSelector();
        FitEditorPropertyRows(objProps->GetWindow());
        StylePropertyControls(objProps->GetWindow());
        objProps->UpdateHeight(LV_SIZE_CONTENT);
        GetCWM()->Update();
        return;
    }
#endif
    if (j["arc"].is_object())
    {
        ArcProperties::UpdateArcProperties(this, j);
    }
    if (j["bar"].is_object())
    {
        BarProperties::UpdateBarProperties(this, j);
    }
    if(j["button"].is_object())
    {
        BtnProperties::UpdateBtnProperties(this, j);
    }
    if (j["btnm"].is_object())
    {
        BtnmProperties::UpdateBtnmProperties(this, j);
    }
#if LVGL_VERSION_MAJOR >= 9
    if (!j["arc"].is_object() && !j["bar"].is_object() && !j["button"].is_object() && !j["btnm"].is_object())
    {
        if (lv_obj_get_class(object) == &lv_image_class) ImageProperties::Update(this, j);
        else BasicProperties::Update(this, j);
    }
    FitEditorPropertyRows(objProps->GetWindow());
    AddFontSelector();
    StylePropertyControls(objProps->GetWindow());
    objProps->UpdateHeight(LV_SIZE_CONTENT);
#endif
    GetCWM()->Update();
}

void PropertyWindow::AddFontSelector()
{
    if(fontSelectorRow)lv_obj_delete(fontSelectorRow);
    auto* content=lv_obj_get_child(objProps->GetWindow(),0);
    auto* row=lv_obj_create(content);lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_style_pad_all(row,4,0);lv_obj_set_style_border_width(row,0,0);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_scrollable(row,false);
    fontSelectorRow=row;lv_obj_add_event_cb(row,[](lv_event_t* e){static_cast<PropertyWindow*>(lv_event_get_user_data(e))->fontSelectorRow=nullptr;},LV_EVENT_DELETE,this);
    auto* label=lv_label_create(row);lv_label_set_text(label,"Font");lv_obj_set_width(label,115);
    auto* dropdown=lv_dropdown_create(row);lv_obj_set_flex_grow(dropdown,1);EditorStyle::Control(dropdown);
    std::string options="Inherit / theme";uint32_t selected=0,index=1;
    const auto id=FontLibrary::Selection(GetSelectedObject());
    for(const auto& font:fonts.ToJSON()){options+='\n';options+=font.at("name").get<std::string>();if(font.at("id")==id)selected=index;++index;}
    if(id.empty()&&lv_obj_get_style_text_font(GetSelectedObject(),LV_PART_MAIN)!=(LV_FONT_DEFAULT)){options+="\nCustom / inherited font";selected=index++;}
    const auto builtinStart=index;
    for(const auto& builtin:BuiltinFonts::Catalog()){options+="\nBuilt-in: ";options+=builtin.name;options+="...";}
    lv_dropdown_set_options(dropdown,options.c_str());lv_dropdown_set_selected(dropdown,selected);
    lv_obj_move_to_index(row,0);
    lv_obj_add_event_cb(dropdown,[](lv_event_t* e){auto* pw=static_cast<PropertyWindow*>(lv_event_get_user_data(e));FocusEditorControl(pw->GetKBGroup(),lv_event_get_target_obj(e));},LV_EVENT_CLICKED,this);
    struct FontChoices {PropertyWindow* properties;uint32_t builtinStart,selected;};
    lv_obj_add_event_cb(dropdown,[](lv_event_t* e){
        auto* choices=static_cast<FontChoices*>(lv_event_get_user_data(e));
        if(lv_event_get_code(e)==LV_EVENT_DELETE){delete choices;return;}
        if(lv_event_get_code(e)!=LV_EVENT_VALUE_CHANGED)return;
        auto* pw=choices->properties;auto* dropdown=lv_event_get_target_obj(e);const auto index=lv_dropdown_get_selected(dropdown);
        if(index>=choices->builtinStart){
            lv_dropdown_set_selected(dropdown,choices->selected);const auto& catalog=BuiltinFonts::Catalog();const auto builtin=index-choices->builtinStart;
            if(builtin<catalog.size())FontConverterWindow::OpenBuiltin(pw,catalog[builtin].id);
        }else if(index<=pw->Fonts().ToJSON().size()){
            choices->selected=index;pw->Fonts().Apply(pw->GetSelectedObject(),index?pw->Fonts().ToJSON()[index-1].at("id").get<std::string>():std::string{});
        }
    },LV_EVENT_ALL,new FontChoices{this,builtinStart,selected});
}

void PropertyWindow::createObjProps()
{
    objProps = new CollapsableWindow(propertyWin, "Object", false, 10, 0, 375, 0);
#if LVGL_VERSION_MAJOR >= 9
    objProps->UpdateHeight(0);
#endif
    cwm->AddWindow(objProps);
}

void PropertyWindow::createGlobalProps()
{
    globalProps = new CollapsableWindow(propertyWin, "Global", false, 10, 0, 380, 75);
    lv_obj_t* win = globalProps->GetWindow();
    lv_obj_t* th_roller = lv_roller_create(win, NULL);
#if LVGL_VERSION_MAJOR >= 9
    lv_roller_set_options(th_roller, "Default", LV_ROLLER_MODE_NORMAL);
#else
    lv_roller_set_options(th_roller, th_options, true);
#endif
    SetEditorEventCallback<theme_select_event_handler>(th_roller);

    lv_obj_t* hue_roller = lv_roller_create(win, NULL);
    lv_roller_set_options(hue_roller, "0\n30\n60\n90\n120\n150\n180\n210\n240\n270\n300\n330", true);
    SetEditorEventCallback<hue_select_event_cb>(hue_roller);
#if LVGL_VERSION_MAJOR >= 9
    for (auto* control : {th_roller, hue_roller})
        lv_obj_add_event_cb(control, [](lv_event_t* event) {
            auto* properties = static_cast<PropertyWindow*>(lv_event_get_user_data(event));
            FocusEditorControl(properties->GetKBGroup(), lv_event_get_target_obj(event));
        }, LV_EVENT_CLICKED, this);
#endif

    lv_obj_t* themeLabel = lv_label_create(win, nullptr);
    lv_obj_t* hueLabel = lv_label_create(win, nullptr);
    lv_label_set_text(themeLabel, "Theme:");
    lv_label_set_text(hueLabel, "Hue:");
    lv_obj_set_pos(themeLabel, 45, 20);
    lv_obj_set_pos(hueLabel, 250, 20);

    lv_obj_set_pos(th_roller, 100, 0);
    lv_obj_set_pos(hue_roller, 280, 0);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_size(win, 370, 62);
    lv_roller_set_visible_row_count(th_roller, 1);
    lv_roller_set_visible_row_count(hue_roller, 1);
    lv_obj_set_size(th_roller, 160, 36);
    lv_obj_set_size(hue_roller, 78, 36);
    lv_obj_set_pos(themeLabel, 0, 0);
    lv_obj_set_pos(hueLabel, 180, 0);
    lv_obj_set_pos(th_roller, 0, 22);
    lv_obj_set_pos(hue_roller, 180, 22);
    lv_roller_set_selected(hue_roller, 4, LV_ANIM_OFF);
#endif
    globalProps->AddObjectToWindow(themeLabel);
    globalProps->AddObjectToWindow(hueLabel);
    globalProps->AddObjectToWindow(th_roller);
    globalProps->AddObjectToWindow(hue_roller);
    cwm->AddWindow(globalProps);
#if LVGL_VERSION_MAJOR >= 9
    globalProps->ToggleCollapsed();
    cwm->Update();
#endif
}

void PropertyWindow::theme_select_event_handler(lv_obj_t* roller, EditorEventCode event)
{
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        // lv_coord_t hres = lv_disp_get_hor_res(NULL);
        // lv_coord_t vres = lv_disp_get_ver_res(NULL);

        uint16_t opt = lv_roller_get_selected(roller);
        if (opt >= themes.size())
            return;
        activeTheme = themes[opt];
#if LVGL_VERSION_MAJOR >= 9
        lv_display_set_theme(lv_display_get_default(), activeTheme);
#else
        lv_theme_set_current(activeTheme);
#endif
    }
}

void PropertyWindow::initStyles()
{
    using namespace Serialization;
    Styles.emplace("lv_style_scr", Style::ToJSON(lv_style_scr, "lv_style_scr"));
    Styles.emplace("lv_style_transp", Style::ToJSON(lv_style_transp, "lv_style_transp"));
    Styles.emplace("lv_style_transp_tight", Style::ToJSON(lv_style_transp_tight, "lv_style_transp_tight"));
    Styles.emplace("lv_style_transp_fit", Style::ToJSON(lv_style_transp_fit, "lv_style_transp_fit"));
    Styles.emplace("lv_style_plain", Style::ToJSON(lv_style_plain, "lv_style_plain"));
    Styles.emplace("lv_style_plain_color", Style::ToJSON(lv_style_plain_color, "lv_style_plain_color"));
    Styles.emplace("lv_style_pretty", Style::ToJSON(lv_style_pretty, "lv_style_pretty"));
    Styles.emplace("lv_style_pretty_color", Style::ToJSON(lv_style_pretty_color, "lv_style_pretty_color"));
    Styles.emplace("lv_style_btn_rel", Style::ToJSON(lv_style_btn_rel, "lv_style_btn_rel"));
    Styles.emplace("lv_style_btn_pr", Style::ToJSON(lv_style_btn_pr, "lv_style_btn_pr"));
    Styles.emplace("lv_style_btn_tgl_rel", Style::ToJSON(lv_style_btn_tgl_rel, "lv_style_btn_tgl_rel"));
    Styles.emplace("lv_style_btn_tgl_pr", Style::ToJSON(lv_style_btn_tgl_pr, "lv_style_btn_tgl_pr"));
    Styles.emplace("lv_style_btn_ina", Style::ToJSON(lv_style_btn_ina, "lv_style_btn_ina"));

    StylePtrs.emplace("lv_style_scr", &lv_style_scr);
    StylePtrs.emplace("lv_style_transp", &lv_style_transp);
    StylePtrs.emplace("lv_style_transp_tight", &lv_style_transp_tight);
    StylePtrs.emplace("lv_style_transp_fit", &lv_style_transp_fit);
    StylePtrs.emplace("lv_style_plain", &lv_style_plain);
    StylePtrs.emplace("lv_style_plain_color", &lv_style_plain_color);
    StylePtrs.emplace("lv_style_pretty", &lv_style_pretty);
    StylePtrs.emplace("lv_style_pretty_color", &lv_style_pretty_color);
    StylePtrs.emplace("lv_style_btn_rel", &lv_style_btn_rel);
    StylePtrs.emplace("lv_style_btn_pr", &lv_style_btn_pr);
    StylePtrs.emplace("lv_style_btn_tgl_rel", &lv_style_btn_tgl_rel);
    StylePtrs.emplace("lv_style_btn_tgl_pr", &lv_style_btn_tgl_pr);
    StylePtrs.emplace("lv_style_btn_ina", &lv_style_btn_ina);
}

void PropertyWindow::hue_select_event_cb(lv_obj_t* roller, EditorEventCode event)
{
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        uint16_t hue = lv_roller_get_selected(roller) * 30;

        initializeThemes(hue);

#if LVGL_VERSION_MAJOR >= 9
        lv_display_set_theme(lv_display_get_default(), themes.front());
#else
        lv_theme_set_current(activeTheme);
#endif
    }
}

void PropertyWindow::createTreeView()
{
#if LVGL_VERSION_MAJOR >= 9
    treeView = new TreeView(304, 36, 220, screenY - 36, "Hierarchy", true, false);
    treeView->AddNode("Screen", simWindow->GetDrawSurface(), 0, true);
    treeView->AddSelectCallback(objSelectCB);
    return;
#endif
    treeWin = new CollapsableWindow(propertyWin, "UI Objects", false, 10, 10, 390, 250);

    treeView = new TreeView(0, 0, 385, 250, "UI Objects", false, false);
    treeView->AddNode("Screen", simWindow->GetDrawSurface(), 0, true);
    treeView->AddSelectCallback(objSelectCB);
    treeView->AddDeleteCallback(deleteCB);

    treeWin->AddObjectToWindow(treeView->GetTreeViewUI());
    cwm->AddWindow(treeWin);
}

/**
 * \brief Called when a node is selected or deselected from the node tree view
 * \param node - The node that is changing state
 */
void PropertyWindow::objSelectCB(TreeNode* node)
{
#if LVGL_VERSION_MAJOR >= 9
    if (node && node->GetLVObject() && lv_obj_get_parent(node->GetLVObject()))
    {
        HighlightEditorObject(node->GetLVObject(), node->IsSelected());
        auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(node->GetLVObject()));
        if (node->IsSelected() && data && data->toolTray)
            data->toolTray->GetPropertyWindow()->SetSelectedObject(node->GetLVObject());
    }
#else
    if (node->IsSelected())
    {
        // Set the node data to the node objects current style
        uiObjData* uidata = nullptr;
        try
        {
            uidata = std::any_cast<uiObjData *>(node->GetNodeData());
        }
        catch (std::bad_any_cast)
        {
        }
        if (uidata == nullptr)
        {
            uidata = new uiObjData();
        }
        uidata->style = node->GetLVObject()->style_p;
        node->SetNodeData(uidata);
        lv_obj_set_style(node->GetLVObject(), &lv_style_plain);
    }
    else
    {
        // We are returning the node to its original style
        uiObjData* uidata = std::any_cast<uiObjData *>(node->GetNodeData());
        if (uidata == nullptr)
        {
            uidata = new uiObjData();
            return;
        }

        lv_obj_set_style(node->GetLVObject(), uidata->style);
    }
#endif
}


/**
 * \brief Deletes the user data from the object (If any) to clean up memory
 * \param node 
 */
void PropertyWindow::deleteCB(TreeNode* node)
{
    if (node == nullptr)
    {
        // This really should never happen
        return;
    }
    uiObjData* uiData = (uiObjData*)lv_obj_get_user_data(node->GetLVObject());
    if (uiData == nullptr)
    {
        return;
    }
}

#pragma region ThemeInit
void PropertyWindow::initializeThemes(uint16_t hue)
{
    themes.clear();
#if LVGL_VERSION_MAJOR >= 9
    themes.push_back(lv_theme_default_init(lv_display_get_default(), lv_color_hex(0x3589bd),
                                           lv_palette_main(LV_PALETTE_BLUE_GREY), true, LV_FONT_DEFAULT));
#else
#if LV_USE_THEME_NIGHT
    themes.push_back(lv_theme_night_init(hue, NULL));
#endif

#if LV_USE_THEME_MATERIAL
    themes.push_back(lv_theme_material_init(hue, NULL));
#endif

#if LV_USE_THEME_ALIEN
    themes.push_back(lv_theme_alien_init(hue, NULL));
#endif

#if LV_USE_THEME_ZEN
    themes.push_back(lv_theme_zen_init(hue, NULL));
#endif

#if LV_USE_THEME_NEMO
    themes.push_back(lv_theme_nemo_init(hue, NULL));
#endif

#if LV_USE_THEME_MONO
    themes.push_back(lv_theme_mono_init(hue, NULL));
#endif

#if LV_USE_THEME_DEFAULT
    themes.push_back(lv_theme_default_init(hue, NULL));
#endif
#endif
}
#pragma endregion

void PropertyWindow::SetSelection(const std::vector<lv_obj_t*>& objects)
{
    const auto available = treeView->GetObjects();
    std::vector<lv_obj_t*> valid;
    for(auto* object : objects)
        if(std::find(available.begin(), available.end(), object) != available.end() && std::find(valid.begin(), valid.end(), object) == valid.end())valid.push_back(object);
    for(auto* object : selectedObjects)if(object && lv_obj_is_valid(object))HighlightEditorObject(object, false);
    selectedObjects = std::move(valid);
    auto* primary = selectedObjects.empty() ? simWindow->GetDrawSurface() : selectedObjects.back();
    SetSelectedObject(primary, false, true);
    treeView->ShowSelection(selectedObjects, primary);
}
std::vector<lv_obj_t*> PropertyWindow::SelectionRoots() const
{
    std::vector<lv_obj_t*> roots;
    for(auto* object : selectedObjects) {
        bool nested = false;
        for(auto* parent = lv_obj_get_parent(object); parent; parent = lv_obj_get_parent(parent))
            if(std::find(selectedObjects.begin(), selectedObjects.end(), parent) != selectedObjects.end()) {nested = true;break;}
        if(!nested)roots.push_back(object);
    }
    return roots;
}
