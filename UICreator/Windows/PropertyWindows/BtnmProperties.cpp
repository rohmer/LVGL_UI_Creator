#include "BtnmProperties.h"

lv_obj_t* BtnmProperties::buttonEditor;


void BtnmProperties::CreateBtnmProperties(PropertyWindow* pw)
{
    pw->ObjectPropWin()->UpdateHeight(250);
    lv_obj_t* cont = lv_cont_create(pw->ObjectPropWin()->GetWindow(), nullptr);
    lv_cont_set_layout(cont, LV_LAYOUT_PRETTY);
    lv_cont_set_fit(cont, LV_FIT_FILL);
    pw->ObjectPropWin()->AddObjectToWindow(cont);

    buttonEditor = lv_btn_create(cont, nullptr);
    lv_obj_set_size(buttonEditor, 300, 50);
    lv_obj_t* btnLabel = lv_label_create(buttonEditor, nullptr);
    std::stringstream ss;
    ss << LV_SYMBOL_EDIT << " Button Map Editor";
    lv_label_set_text(btnLabel, ss.str().c_str());
    lv_obj_set_user_data(buttonEditor, pw);
    lv_obj_set_event_cb(buttonEditor, buttonEditorCB);
}

void BtnmProperties::UpdateBtnmProperties(PropertyWindow* pw, json j)
{
    pw->Drawing(true);
    if (pw->CurrentlyLoadedProp != PropertyWindow::eObjType::BUTTONMATRIX)
    {
        pw->ObjectPropWin()->DeleteChildren();
        CreateBtnmProperties(pw);
        pw->CurrentlyLoadedProp = PropertyWindow::BUTTONMATRIX;
    }

    pw->Drawing(false);
}

void BtnmProperties::buttonEditorCB(lv_obj_t* obj, lv_event_t event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    PropertyWindow* pw = (PropertyWindow *)lv_obj_get_user_data(obj);
    BtnmEditorWindow::CreateBtnmEditorWindow(pw);
}