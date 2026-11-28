#include "ToolTray.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorDrag.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorEvents.h"
#include "../Utils/ObjectIcons.h"
#include "PropertyWindow.h"
#include "ToolBar.h"
#include "../ObjectCreation/BasicWidget.h"
#include "../ObjectCreation/AdvancedWidget.h"
#include "../ObjectCreation/CustomWidget.h"
#include "../../3rdParty/LVGL/lvgl/include/lvgl/widgets/lv_bootstrap.h"
#include <unordered_map>
#include <algorithm>
#undef GetObject
namespace {
void StylePaletteCard(lv_obj_t* button) {
    lv_obj_set_style_pad_all(button, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 7, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(0x526172), LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(0x65afff), LV_STATE_HOVERED);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x293647), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x365f79), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_hex(0xe4eaf2), LV_PART_MAIN);
    lv_obj_set_style_outline_color(button, lv_color_hex(0x45d6b3), LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(button, 2, LV_STATE_FOCUS_KEY);
}
void LayoutPalette(lv_obj_t* page, int minimumWidth) {
    const int available = lv_obj_get_content_width(page);
    if (available < 20) return;
    constexpr int gap = 6;
    const int columns = (std::max)(1, (std::min)(3, (available+gap)/(minimumWidth+gap)));
    const int width = (available-gap*(columns-1))/columns;
    const bool row = columns == 1;
    for (uint32_t index=0; index<lv_obj_get_child_count(page); ++index) {
        auto* button = lv_obj_get_child(page,index);
        if (!lv_obj_has_class(button,&lv_button_class)) continue;
        int cardHeight = row ? 64 : 100;
        lv_obj_set_size(button,width,cardHeight);
        for (uint32_t part=0; part<lv_obj_get_child_count(button); ++part) {
            auto* object = lv_obj_get_child(button,part);
            if (ObjectIcons::Get(object)) lv_obj_align(object,row?LV_ALIGN_LEFT_MID:LV_ALIGN_TOP_MID,row?4:0,row?0:2);
            else if (lv_obj_has_class(object,&lv_label_class)) {
                const int textWidth = (std::max)(1, row ? width - 56 : width - 12);
                lv_obj_set_width(object,textWidth);
                lv_point_t textSize{};
                lv_text_get_size(&textSize, lv_label_get_text(object), lv_obj_get_style_text_font(object, LV_PART_MAIN),
                    lv_obj_get_style_text_letter_space(object, LV_PART_MAIN), lv_obj_get_style_text_line_space(object, LV_PART_MAIN), textWidth, LV_TEXT_FLAG_NONE);
                cardHeight = (std::max)(cardHeight, textSize.y + (row ? 16 : 56));
                lv_obj_set_style_text_align(object,row?LV_TEXT_ALIGN_LEFT:LV_TEXT_ALIGN_CENTER,LV_PART_MAIN);
                lv_obj_align(object,row?LV_ALIGN_LEFT_MID:LV_ALIGN_BOTTOM_MID,row?44:0,0);
            }
        }
        lv_obj_set_height(button, cardHeight);
    }
}
void MakeResponsivePalette(lv_obj_t* page, int minimumWidth) {
    lv_obj_set_style_pad_row(page,6,LV_PART_MAIN); lv_obj_set_style_pad_column(page,6,LV_PART_MAIN);
    lv_obj_add_event_cb(page,[](lv_event_t* event) {
        LayoutPalette(lv_event_get_current_target_obj(event),static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event))));
    },LV_EVENT_SIZE_CHANGED,reinterpret_cast<void*>(static_cast<intptr_t>(minimumWidth)));
    LayoutPalette(page,minimumWidth);
}
}
ToolTray::ToolTray(lv_obj_t* parent,
                   TreeView* objectTree,
                   lv_obj_t* drawSurface,
                   PropertyWindow* propWin,
                   ToolBar* toolBar)
{
    objTree = objectTree;
    minWin = new MinimizableWindow("Tool Tray",
                                   0,
                                   0,
                                   lv_obj_get_width_fit(parent) / 4,
                                   lv_obj_get_height_fit(parent),
                                   nullptr,
                                   false,
                                   lv_obj_get_width_fit(parent) / 2 - 40,
                                   10);
    this->toolBar = toolBar;
    toolWin = minWin->GetWindowObj();
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_width(toolWin, 300);
#endif
    this->drawSurface = drawSurface;
    this->propertyWindow = propWin;
    Serialization::LVObject::FromJSON(Serialization::LVObject::ToJSON(drawSurface), drawSurface);
    auto* screenData = static_cast<ObjectUserData*>(lv_obj_get_user_data(drawSurface));
    screenData->objectName = "Screen";
    screenData->toolTray = this;
    EditorWindow::SetTitle(toolWin, "Tool Tray");
    lv_obj_align(toolWin, nullptr, LV_ALIGN_IN_LEFT_MID, 0, 0);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_align(toolWin, LV_ALIGN_TOP_LEFT, 0, 36);
#endif
    initializeToolMatrix();
}

#pragma region Tool Matrix
void ToolTray::initializeToolMatrix()
{
    lv_obj_t* tabView = lv_tabview_create(EditorWindow::Content(toolWin), nullptr);
    lv_obj_t* widgetTab = lv_tabview_add_tab(tabView, "Widgets");
    lv_obj_t* advancedTab = lv_tabview_add_tab(tabView, "Advanced\nWidgets");
    lv_obj_t* dialogTab = lv_tabview_add_tab(tabView, "Dialogs");
    lv_obj_t* customTab = lv_tabview_add_tab(tabView, "Custom");
    lv_obj_t* bootstrapTab = lv_tabview_add_tab(tabView, "Bootstrap");
    lv_obj_t* templatesTab = lv_tabview_add_tab(tabView, "Templates");
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_size(tabView, LV_PCT(100), LV_PCT(100));
    lv_tabview_set_tab_bar_size(tabView, 96);
    auto* categoryBar = lv_tabview_get_tab_bar(tabView);
    lv_obj_set_flex_flow(categoryBar, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(categoryBar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(categoryBar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(categoryBar, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(categoryBar, false);
    lv_obj_set_scrollbar_mode(categoryBar, LV_SCROLLBAR_MODE_OFF);
    for(uint32_t index = 0; index < lv_obj_get_child_count(categoryBar); ++index) {
        auto* category = lv_obj_get_child(categoryBar, index);
        lv_obj_set_flex_grow(category, 0);
        lv_obj_set_size(category, LV_PCT(33), 44);
        lv_obj_set_style_pad_all(category, 4, LV_PART_MAIN);
    }
    for (auto* tab : {widgetTab, advancedTab, dialogTab, customTab, bootstrapTab, templatesTab})
    {
        lv_obj_set_style_pad_all(tab, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_row(tab, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_column(tab, 4, LV_PART_MAIN);
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_scroll_dir(tab, LV_DIR_VER);
    }
#endif
    initializeWidgetButtons(widgetTab, 0);
    initializeWidgetButtons(advancedTab, 1);
    initializeWidgetButtons(dialogTab, 2);
    initializeWidgetButtons(customTab, 3);
    initializeBootstrapButtons(bootstrapTab, false);
    initializeBootstrapButtons(templatesTab, true);
}

void ToolTray::initializeBootstrapButtons(lv_obj_t* parent, bool templates)
{
    auto* hint = lv_label_create(parent);
    lv_label_set_text(hint, templates ? "Editable native starter layouts" : "Bootstrap-inspired native components");
    lv_obj_set_width(hint, LV_PCT(100));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_WRAP);
    for(int preset = templates ? LV_BOOTSTRAP_COMPONENT_COUNT : 0;
        preset < (templates ? LV_BOOTSTRAP_PRESET_COUNT : LV_BOOTSTRAP_COMPONENT_COUNT); ++preset)
    {
        struct Binding { ToolTray* tray; int preset; };
        auto* button = lv_button_create(parent);
        lv_obj_set_size(button, LV_PCT(48), 100);
        StylePaletteCard(button);
        auto* caption = lv_label_create(button);
        lv_label_set_text(caption, lv_bootstrap_preset_name(static_cast<lv_bootstrap_preset_t>(preset)));
        lv_obj_set_width(caption, LV_PCT(100));
        lv_label_set_long_mode(caption, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(caption,LV_ALIGN_BOTTOM_MID,0,0);
        auto* icon = ObjectIcons::Create(button,ObjectIcons::Preset(preset));
        lv_obj_align(icon,LV_ALIGN_TOP_MID,0,2);
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
            if(lv_event_get_code(event) == LV_EVENT_DELETE) delete binding;
            else if(lv_event_get_code(event) == LV_EVENT_CLICKED) binding->tray->CreateBootstrapPreset(binding->preset);
        }, LV_EVENT_ALL, new Binding{this, preset});
    }
    MakeResponsivePalette(parent,108);
}

lv_obj_t* ToolTray::CreateBootstrapTemplate(const std::string& id)
{
    return CreateBootstrapPreset(id == "signin" ? LV_BOOTSTRAP_SIGN_IN :
        id == "gallery" ? LV_BOOTSTRAP_COMPONENT_GALLERY : LV_BOOTSTRAP_DASHBOARD);
}

lv_obj_t* ToolTray::CreateBootstrapPreset(int preset, bool dark)
{
    if(preset < 0 || preset >= LV_BOOTSTRAP_PRESET_COUNT) return nullptr;
    EditorEdit::Scope edit(this);
    auto* parent = drawSurface;
    if(auto* selected = objTree->GetSelectedNode())
        if(selected->GetLVObject() && selected->GetLVObject() != lv_screen_active()) parent = selected->GetLVObject();
    if(preset >= LV_BOOTSTRAP_COMPONENT_COUNT) parent = drawSurface;
    parent = Serialization::CustomWidgets::ContentParent(parent);
    parent = Serialization::ExtendedWidgets::ContentParent(parent);
    if(Serialization::AdvancedKeyboard::Type(parent))parent=lv_obj_get_parent(parent);
    if(Serialization::AdvancedControls::Type(parent))parent=lv_obj_get_parent(parent);
    using Created = std::vector<std::pair<lv_obj_t*, std::string>>;
    Created created;
    auto* root = lv_bootstrap_create(parent, static_cast<lv_bootstrap_preset_t>(preset), dark,
        [](lv_obj_t* object, const char* name, void* context) {
            static_cast<Created*>(context)->emplace_back(object, name);
        }, &created);
    if(!root) return nullptr;
    std::unordered_map<lv_obj_t*, unsigned> ids;
    for(auto* object : objTree->GetObjects())
        if(auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object))) ids.emplace(object, data->objectID);
    ids.emplace(drawSurface, 0);
    for(const auto& entry : created)
    {
        auto* object = entry.first;
        Serialization::LVObject::FromJSON({{"name", entry.second}}, object);
        auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
        data->objectName = entry.second;
        if (object == root)
            if (const auto* icon = ObjectIcons::Preset(preset))
                data->objectJson["base"]["design"]["paletteIcon"] = icon->key;
        if(const auto* action = lv_bootstrap_get_action(object)) data->objectJson["base"]["bootstrapAction"] = action;
        auto* ancestor = lv_obj_get_parent(object);
        while(ancestor && !ids.count(ancestor)) ancestor = lv_obj_get_parent(ancestor);
        AdoptWidget(object, ancestor ? ids.at(ancestor) : 0);
        if(onWidgetCreated)onWidgetCreated(object);
        ids[object] = data->objectID;
        data->objectJson = Serialization::ObjectSerializer::SerializeObject(object);
        data->objectJson["base"]["drag"] = true;
    }
    lv_obj_update_layout(root);
    lastWidget = root;
    propertyWindow->SetSelectedObject(root);
    return root;
}

#pragma region Widget Buttons
void ToolTray::initializeWidgetButtons(lv_obj_t* parent, int category)
{
    int width = lv_obj_get_width_fit(parent) / 3 - 5;
    int height = 61;

    std::string bNames[40] = {
        "Arc", "Bar", "Button", "Button matrix", "Calendar", "Canvas", "Check box",
        "Chart", "Container", "Drop down list", "Gauge", "Image", "Image button", "Keyboard", "Label", "LED",
        "Line", "Line meter", "Message box", "Page", "Preloader", "Roller", "Slider", "Spinbox", "Switch",
        "Table", "Tabview", "Text area", "Tile view", "Window", "Text box", "Layer view", "Barcode", "QR code",
        "", "Radio Group", "Checkbox", "Checkbox nested in Label", "Date Picker", "Menu"
    };
    int xCtr = 0;
    int yCtr = 0;

    for (int i = 0; i < 40; i++)
    {
        if (i == 34) continue; // Reserved for imported legacy Advanced Keyboard records.
        const bool isAdvanced = i == 3 || i == 4 || i == 5 || i == 7 || i == 10 ||
            i == 12 || i == 13 || i == 17 || i == 18 || i == 19 || i == 25 ||
            i == 26 || i == 28 || i == 29 || i == 32 || i == 33 || i >= 35;
        const int itemCategory = i == 30 || i == 31 ? 3 : i == 18 || i == 29 ? 2 : isAdvanced ? 1 : 0;
        if (itemCategory != category)
            continue;
        lv_obj_t* button = lv_btn_create(parent, nullptr);
        lv_obj_set_user_data(button, (lv_obj_user_data_t)bNames[i].c_str());
        int x = (width * xCtr) + (5 * xCtr) + 4;
        int y = (height * yCtr) + (5 * yCtr) + 5;
        lv_obj_set_size(button, width, height);
        lv_obj_set_pos(button, x, y);


        lv_obj_t* bLabel = lv_label_create(button, nullptr);
        lv_label_set_text(bLabel, bNames[i].c_str());
#if LVGL_VERSION_MAJOR >= 9
        lv_obj_set_width(button, LV_PCT(31));
        lv_obj_set_height(button, 100);
        StylePaletteCard(button);
        lv_obj_set_width(bLabel, LV_PCT(100));
        lv_label_set_long_mode(bLabel, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_style_text_align(bLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(bLabel, LV_ALIGN_BOTTOM_MID, 0, -2);
        auto* icon = ObjectIcons::Create(button,ObjectIcons::Widget(i));
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 2);
#endif
        xCtr++;
        if (xCtr > 2)
        {
            xCtr = 0;
            yCtr++;
        }
        sCB* callbackStruct = new sCB();
        callbackStruct->toolTray = this;
        callbackStruct->typeID = i;
        lv_obj_set_user_data(button, static_cast<lv_obj_user_data_t>(callbackStruct));
        SetEditorEventCallback<create_obj_cb>(button);
#if LVGL_VERSION_MAJOR >= 9
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            delete static_cast<sCB*>(lv_event_get_user_data(event));
        }, LV_EVENT_DELETE, callbackStruct);
#endif
        widgetButtons.push_back(button);
    }
    MakeResponsivePalette(parent,88);
}


#pragma endregion
#pragma endregion

#pragma region Callback
void ToolTray::create_obj_cb(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev == LV_EVENT_CLICKED)
    {
        sCB* callbackStruct = static_cast<sCB*>(lv_obj_get_user_data(obj));
        int objID = callbackStruct->typeID;
        ToolTray* tt = callbackStruct->toolTray;
        TreeView* tv = tt->objTree;
        TreeNode* node = tv->GetSelectedNode();
        lv_obj_t *parent, *newObj;
        bool compositeSibling = false;
        if (node != nullptr)
        {
            parent = tv->GetSelectedNode()->GetLVObject();
            if (parent == nullptr || parent == lv_screen_active())
                parent = tt->drawSurface;
        }
        else
        {
            parent = tt->drawSurface;
        }
#if LVGL_VERSION_MAJOR >= 9
        parent = Serialization::CustomWidgets::ContentParent(parent);
        parent = Serialization::ExtendedWidgets::ContentParent(parent);
        if(Serialization::AdvancedKeyboard::Type(parent))parent=lv_obj_get_parent(parent);
        if(Serialization::AdvancedControls::Type(parent)) { parent=lv_obj_get_parent(parent); compositeSibling=true; }
#endif
        int x = 0, y = 0;
        if (tt->lastWidget != nullptr)
        {
            x = lv_obj_get_x(tt->lastWidget) + 10;
            y = lv_obj_get_y(tt->lastWidget) + 10;
        }
        newObj = nullptr;
        int parID = -1;
        if (tt->objTree->GetSelectedNode() != nullptr)
            parID = tt->objTree->GetSelectedNode()->GetID();
        else
            parID = 0;
#if LVGL_VERSION_MAJOR >= 9
        if (compositeSibling)
        {
            parID = 0;
            for (auto* ancestor = parent; ancestor && ancestor != tt->drawSurface; ancestor = lv_obj_get_parent(ancestor))
                if (auto* parentNode = tv->GetNodeByObject(ancestor)) { parID = static_cast<int>(parentNode->GetID()); break; }
        }
#endif
        ObjectUserData* userData = new ObjectUserData();
        userData->parentID = parID;
        switch (objID)
        {
        case 0:
            newObj = Arc::Create(parent, x, y);
            tt->objTree->AddNode("Arc", newObj, parID, false);
            userData->objectName = "Arc";
            break;
        case 1:
            newObj = Bar::Create(parent, x, y);
            tt->objTree->AddNode("Bar", newObj, parID, false);
            userData->objectName = "Bar";
            break;
        case 2:
            newObj = Button::Create(parent, x, y);
            tt->objTree->AddNode("Button", newObj, parID, false);
            userData->objectName = "Button";
            break;
        case 3:
            newObj = ButtonMatrix::Create(parent, x, y);
            tt->objTree->AddNode("ButtonMatrix", newObj, parID, false);
            userData->objectName = "ButtonMatrix";
            break;
        case 4:
            newObj = Calendar::Create(parent, x, y);
            tt->objTree->AddNode("Calendar", newObj, parID, false);
            userData->objectName = "Calendar";
            break;
#if LVGL_VERSION_MAJOR >= 9
        default:
            newObj = BasicWidget::Create(objID, parent, x, y, userData->objectName);
            if (!newObj) newObj = AdvancedWidget::Create(objID, parent, x, y, userData->objectName);
            if (!newObj) newObj = CustomWidget::Create(objID, parent, x, y, userData->objectName);
            if (newObj)
                tt->objTree->AddNode(userData->objectName, newObj, parID, false);
            break;
#endif
        }
        if (newObj == nullptr)
        {
            delete userData;
            return;
        }
        tt->lastWidget = newObj;
#if LVGL_VERSION_MAJOR >= 9
        bool ownsUserData = lv_obj_get_user_data(newObj) == nullptr;
        if (!ownsUserData)
        {
            const auto name = userData->objectName;
            delete userData;
            userData = static_cast<ObjectUserData*>(lv_obj_get_user_data(newObj));
            userData->objectName = name;
            userData->parentID = parID;
        }
        lv_obj_update_layout(parent);
        lv_obj_update_layout(newObj);
        lv_obj_set_pos(newObj, std::clamp(x, 0, std::max(0, int(lv_obj_get_content_width(parent) - lv_obj_get_width(newObj)))),
            std::clamp(y, 0, std::max(0, int(lv_obj_get_content_height(parent) - lv_obj_get_height(newObj)))));
        lv_obj_set_clickable(newObj, true);
        lv_obj_set_press_lock(newObj, true);
#endif
#if LVGL_VERSION_MAJOR < 9
        lv_obj_set_drag(newObj, true);
        lv_obj_set_style(newObj, &lv_style_plain);
        lv_obj_set_protect(newObj, LV_PROTECT_PRESS_LOST);
        lv_obj_set_top(newObj, true);
#endif
        userData->objectID = tt->currentID++;
        userData->toolTray = tt;

        lv_obj_set_user_data(newObj, static_cast<lv_obj_user_data_t>(userData));
        userData->objectJson = Serialization::ObjectSerializer::SerializeObject(newObj);
        userData->objectJson["base"]["drag"] = true;
#if LVGL_VERSION_MAJOR >= 9
        tt->BindWidget(newObj, ownsUserData);
#else
        tt->BindWidget(newObj, true);
#endif
        tt->RefreshKeyboardBindings();
        if(tt->onWidgetCreated)tt->onWidgetCreated(newObj);
        tt->propertyWindow->SetSelectedObject(newObj, false);
    }
}

void ToolTray::updateProperties(lv_obj_t* obj, EditorEventCode ev)
{
#if LVGL_VERSION_MAJOR < 9
    if (ev == LV_EVENT_DRAG_END)
    {
        ObjectUserData* objectData = static_cast<ObjectUserData*>(lv_obj_get_user_data(obj));
        objectData->toolTray->propertyWindow->SetSelectedObject(obj);
    }
#endif
    if (ev != LV_EVENT_CLICKED)
        return;

    ObjectUserData* objectData = static_cast<ObjectUserData *>(lv_obj_get_user_data(obj));
    objectData->toolTray->propertyWindow->SetSelectedObject(obj);
    objectData->toolTray->toolBar->SetSelectedObject();
}

#if LVGL_VERSION_MAJOR >= 9
void ToolTray::BindContent(lv_obj_t* object)
{
    struct ContentBinding { bool attached = false; };
    auto& state = Serialization::WidgetData<ContentBinding>(object);
    if (!state.attached)
    {
        state.attached = true;
        lv_obj_add_event_cb(object, [](lv_event_t* event) {
            const auto code = lv_event_get_code(event);
            auto* current = lv_event_get_current_target_obj(event);
            if (code == LV_EVENT_CHILD_CREATED)
            {
                for (uint32_t i = 0; i < lv_obj_get_child_count(current); ++i) BindContent(lv_obj_get_child(current, i));
            }
            else if (code == LV_EVENT_CLICKED)
            {
                // Document roots already have updateProperties. Handle an internal
                // control only once, at its original event target.
                const auto* currentData = static_cast<const ObjectUserData*>(lv_obj_get_user_data(current));
                if ((currentData && currentData->toolTray) || lv_event_get_target_obj(event) != current) return;
                for (auto* ancestor = lv_event_get_target_obj(event); ancestor; ancestor = lv_obj_get_parent(ancestor))
                {
                    // Typing controls retain the selected external field and
                    // its keyboard group focus while entering text.
                    if(Serialization::AdvancedKeyboard::Type(ancestor))return;
                    auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(ancestor));
                    if (!data || !data->toolTray) continue;
                    auto* tray = data->toolTray;
                    tray->propertyWindow->SetSelectedObject(ancestor);
                    tray->toolBar->SetSelectedObject();
                    break;
                }
            }
        }, LV_EVENT_ALL, nullptr);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) BindContent(lv_obj_get_child(object, i));
}
#endif

void ToolTray::BindWidget(lv_obj_t* object, bool ownsUserData)
{
#if LVGL_VERSION_MAJOR >= 9
    if (Serialization::AdvancedControls::Type(object)) ui_adv_set_interactive(object, false);
    BindContent(object);
    struct Binding { ToolTray* tray; bool owns; };
    lv_obj_add_event_cb(object, [](lv_event_t* event) {
        auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
        auto* object = lv_event_get_target_obj(event);
        binding->tray->keyboardTargets.erase(object);
        for(auto& target:binding->tray->keyboardTargets)if(target.second.second==object)target.second.second=nullptr;
        if (binding->tray->lastWidget == object) binding->tray->lastWidget = nullptr;
        binding->tray->propertyWindow->ForgetObject(object);
        binding->tray->objTree->ForgetObject(object);
        if (binding->owns) delete static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
        delete binding;
    }, LV_EVENT_DELETE, new Binding{this, ownsUserData});
    lv_obj_add_event_cb(object, [](lv_event_t* event) {
        if (lv_event_get_key(event) != LV_KEY_DEL) return;
        auto* tray = static_cast<ToolTray*>(lv_event_get_user_data(event));
        lv_async_call([](void* context) { static_cast<PropertyWindow*>(context)->DeleteSelectedObject(); }, tray->propertyWindow);
    }, LV_EVENT_KEY, this);
    lv_obj_add_event_cb(object,[](lv_event_t* e){EditorEdit::Scope edit(lv_event_get_target_obj(e));},LV_EVENT_VALUE_CHANGED,nullptr);
    lv_obj_add_event_cb(object,[](lv_event_t* event){
        const auto code=lv_event_get_code(event);
        if(code==LV_EVENT_SIZE_CHANGED||code==LV_EVENT_STYLE_CHANGED||code==LV_EVENT_VALUE_CHANGED||code==LV_EVENT_CHILD_CHANGED){
            auto* tray=static_cast<ToolTray*>(lv_event_get_user_data(event));if(tray->onWidgetChanged)tray->onWidgetChanged();
        }
    },LV_EVENT_ALL,this);
    EditorDrag::Attach(object, object, [](lv_obj_t* widget) {
        auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(widget));
        data->toolTray->propertyWindow->SetSelectedObject(widget);
    }, true);
#endif
    SetEditorEventCallback<updateProperties>(object);
}

void ToolTray::AdoptWidget(lv_obj_t* object, unsigned int parentID)
{
    auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
    data->toolTray = this;
    data->parentID = parentID;
    data->objectID = objTree->AddNode(data->objectName.empty() ? "Widget" : data->objectName, object, parentID, false);
    BindWidget(object, false);
}
void ToolTray::RefreshKeyboardBindings()
{
#if LVGL_VERSION_MAJOR >= 9
    const auto objects=objTree->GetObjects();
    auto entry=[](lv_obj_t* object)->lv_obj_t* {
        if(lv_obj_has_class(object,&lv_textarea_class))return object;
        const auto* kind=Serialization::CustomWidgets::Type(object);
        return kind && std::string(kind)=="textBox"?Serialization::CustomWidgets::Part(object,"Entry"):nullptr;
    };
    for(auto* keyboard:objects) {
        if(!Serialization::AdvancedKeyboard::Type(keyboard))continue;
        auto settings=Serialization::AdvancedKeyboard::ToJSON(keyboard).at("advancedKeyboard");
        const int id=settings.value("targetOid",-1);
        if(id<0){keyboardTargets.erase(keyboard);continue;}
        auto anchor=keyboardTargets.find(keyboard);lv_obj_t* owner=nullptr;
        if(anchor!=keyboardTargets.end() && anchor->second.first==id)owner=anchor->second.second;
        else {
            if(lv_advanced_keyboard_bound_textarea_count(keyboard)==1) {
                auto* bound=lv_advanced_keyboard_bound_textarea(keyboard,0);
                for(auto* candidate:objects)if(entry(candidate)==bound){owner=candidate;break;}
            }
            if(!owner && size_t(id)<objects.size() && entry(objects[id]))owner=objects[id];
        }
        const auto found=std::find(objects.begin(),objects.end(),owner);
        if(found==objects.end()) {
            if(anchor!=keyboardTargets.end()) {
                propertyWindow->Tries().ApplySettings(keyboard,{{"targetOid",-1},{"autoFocus",true}});
                keyboardTargets.erase(keyboard);
            }
        } else {
            const int next=static_cast<int>(std::distance(objects.begin(),found));
            if(next!=id)propertyWindow->Tries().ApplySettings(keyboard,{{"targetOid",next}});
            keyboardTargets[keyboard]={next,owner};
        }
    }
    Serialization::AdvancedKeyboard::BindTargets(drawSurface,objects);
#endif
}
#pragma endregion
