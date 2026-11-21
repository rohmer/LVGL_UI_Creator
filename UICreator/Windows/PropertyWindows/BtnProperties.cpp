#include "BtnProperties.h"
#include "../../Utils/EditorControls.h"
#include "../../Utils/EditorEvents.h"
lv_obj_t *BtnProperties::inkIn,  *BtnProperties::inkOut,  *BtnProperties::inkWait,  *BtnProperties::toggle,  *BtnProperties::layout,  *BtnProperties::fitTop,  *BtnProperties::fitBot,  *BtnProperties::fitRight,  *BtnProperties::fitLeft;
lv_obj_t *BtnProperties::rel,  *BtnProperties::ina,  *BtnProperties::pr,  *BtnProperties::tglPr,  *BtnProperties::tglRel;
lv_obj_t* BtnProperties::text;

void BtnProperties::CreateBtnProperties(PropertyWindow* pw)
{
    pw->ObjectPropWin()->UpdateHeight(450);
    lv_obj_t* cont = lv_cont_create(pw->ObjectPropWin()->GetWindow(), nullptr);
    lv_cont_set_layout(cont, LV_LAYOUT_PRETTY);
    lv_cont_set_fit(cont, LV_FIT_FILL);
    pw->ObjectPropWin()->AddObjectToWindow(cont);
#if LVGL_VERSION_MAJOR >= 9
    auto* textLabel = lv_label_create(cont);
    lv_label_set_text(textLabel, "Text");
    text = lv_textarea_create(cont);
    lv_obj_set_size(text, LV_PCT(100), 36);
    lv_textarea_set_one_line(text, true);
    lv_obj_add_event_cb(text, [](lv_event_t* event) {
        auto* window = static_cast<PropertyWindow*>(lv_event_get_user_data(event));
        auto* field = lv_event_get_target_obj(event);
        if (lv_event_get_code(event) == LV_EVENT_CLICKED)
        {
            FocusEditorControl(window->GetKBGroup(), field);
        }
        else if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED && !window->Drawing() && window->GetSelectedObject())
        {
            EditorEdit::Scope edit(field, true);
            Serialization::LVButton::SetText(window->GetSelectedObject(), lv_textarea_get_text(field));
        }
    }, LV_EVENT_ALL, pw);
#endif
    lv_obj_t* props1 = lv_cont_create(cont, nullptr);
    lv_cont_set_layout(props1, LV_LAYOUT_PRETTY);
    lv_obj_set_size(props1, 370, 50);
    inkIn = PropertyControls::createNumericEntry(pw, props1, "Ink In", "/button/ink/in");
    inkOut = PropertyControls::createNumericEntry(pw, props1, "Ink Out", "/button/ink/out");
    inkWait = PropertyControls::createNumericEntry(pw, props1, "Ink Wait", "/button/ink/wait");
    lv_obj_t* props2 = lv_cont_create(cont, nullptr);
    lv_cont_set_layout(props2, LV_LAYOUT_PRETTY);
    lv_obj_set_size(props2, 370, 100);

    toggle = lv_cb_create(props2, nullptr);
    lv_cb_set_text(toggle, "Toggle");

    lv_obj_t* lLab = lv_label_create(props2, nullptr);
    lv_label_set_text(lLab, "Layout");

    layout = lv_ddlist_create(props2, nullptr);
    lv_ddlist_set_options(layout, "Off\nCenter\nColumn Left\nColumn Mid\nColumn Right\nRow Top\nRow Mid\nRow Bottom\nPretty\nGrid");
    lv_obj_set_user_data(layout, pw);
    SetEditorEventCallback<layoutCB>(layout);

    std::string fits = "None\nTight\nFlood\nFill";

    lv_obj_t* props3 = lv_cont_create(cont, nullptr);
    lv_cont_set_layout(props3, LV_LAYOUT_PRETTY);
    lv_obj_set_size(props3, 370, 100);
    lv_obj_t* fLeft= lv_label_create(props3, nullptr);
    lv_label_set_text(fLeft, "Fit Left");
    fitLeft = lv_ddlist_create(props3, nullptr);
    lv_ddlist_set_options(fitLeft, fits.c_str());
    sFitStruct* lfs = new sFitStruct();
    lfs->fitNum = 0;
    lfs->pw = pw;
    lv_obj_set_user_data(fitLeft, lfs);
    SetEditorEventCallback<fitCB>(fitLeft);
    lv_obj_t* fTop = lv_label_create(props3, nullptr);
    lv_label_set_text(fTop, "Fit Top");
    fitTop= lv_ddlist_create(props3, nullptr);
    lv_ddlist_set_options(fitTop, fits.c_str());
    sFitStruct* tfs = new sFitStruct();
    tfs->fitNum = 1;
    tfs->pw = pw;
    lv_obj_set_user_data(fitTop, tfs);
    SetEditorEventCallback<fitCB>(fitTop);
    lv_obj_t* props4 = lv_cont_create(cont, nullptr);
    lv_cont_set_layout(props4, LV_LAYOUT_PRETTY);
    lv_obj_set_size(props4, 370, 100);
    lv_obj_t* fright = lv_label_create(props4, nullptr);
    lv_label_set_text(fright, "Fit Right");
    fitRight = lv_ddlist_create(props4, nullptr);
    lv_ddlist_set_options(fitRight, fits.c_str());
    sFitStruct* rfs = new sFitStruct();
    rfs->fitNum = 2;
    rfs->pw = pw;
    lv_obj_set_user_data(fitRight, rfs);
    SetEditorEventCallback<fitCB>(fitRight);
    lv_obj_t* fBot = lv_label_create(props4, nullptr);
    lv_label_set_text(fBot, "Fit Bottom");
    fitBot = lv_ddlist_create(props4, nullptr);
    lv_ddlist_set_options(fitBot, fits.c_str());
    sFitStruct* bfs = new sFitStruct();
    bfs->fitNum = 3;
    bfs->pw = pw;
    lv_obj_set_user_data(fitBot, bfs);
    SetEditorEventCallback<fitCB>(fitBot);
}

void BtnProperties::fitCB(lv_obj_t* obj, EditorEventCode event)
{
    sFitStruct* fs = (sFitStruct*)lv_obj_get_user_data(obj);
    if (fs->pw->Drawing())
        return;
    if (event == LV_EVENT_CLICKED)
    {
        lv_group_t* kbGroup = fs->pw->GetKBGroup();
#if LVGL_VERSION_MAJOR >= 9
        FocusEditorControl(kbGroup, obj);
#else
        lv_group_remove_all_objs(kbGroup);
        lv_group_add_obj(kbGroup, obj);
#endif
    }

    if (event == LV_EVENT_VALUE_CHANGED && fs->fitNum >= 0 && fs->fitNum < 4)
    {
        const char* properties[] = {"/button/fit/left", "/button/fit/top", "/button/fit/right", "/button/fit/bottom"};
        Serialization::LVButton::SetValue(fs->pw->GetSelectedObject(), properties[fs->fitNum],
                                          static_cast<int>(lv_ddlist_get_selected(obj)));
    }
}

void BtnProperties::layoutCB(lv_obj_t* obj, EditorEventCode event)
{
    auto* window = static_cast<PropertyWindow*>(lv_obj_get_user_data(obj));
    if (!window || window->Drawing())
        return;
    if (event == LV_EVENT_CLICKED)
    {
        lv_group_t* kbGroup = window->GetKBGroup();
#if LVGL_VERSION_MAJOR >= 9
        FocusEditorControl(kbGroup, obj);
#else
        lv_group_remove_all_objs(kbGroup);
        lv_group_add_obj(kbGroup, obj);
#endif
    }
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        if (auto* selected = window->GetSelectedObject())
            Serialization::LVButton::SetValue(selected, "/button/layout", static_cast<int>(lv_ddlist_get_selected(obj)));
    }
}


void BtnProperties::UpdateBtnProperties(PropertyWindow* pw, json j)
{
    pw->Drawing(true);
    if (pw->CurrentlyLoadedProp != PropertyWindow::eObjType::BUTTON)
    {
        pw->ObjectPropWin()->DeleteChildren();
        CreateBtnProperties(pw);
        pw->CurrentlyLoadedProp = PropertyWindow::eObjType::BUTTON;
    }

#if LVGL_VERSION_MAJOR >= 9
    lv_textarea_set_text(text, j["button"].value("text", std::string{}).c_str());
#endif
    pw->Drawing(false);
}
