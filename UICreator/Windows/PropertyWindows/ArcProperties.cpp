#include "ArcProperties.h"
lv_obj_t *ArcProperties::arcStartTA, *ArcProperties::arcEndTA, *ArcProperties::arcLineWidth, *ArcProperties::
         arcLineRound;
ColorPicker* ArcProperties::cp;

void ArcProperties::CreateArcProperties(PropertyWindow* pw)
{
    pw->ObjectPropWin()->UpdateHeight(300);
    lv_obj_t* cont = lv_cont_create(pw->ObjectPropWin()->GetWindow(), nullptr);
    lv_cont_set_layout(cont, LV_LAYOUT_PRETTY);
    lv_cont_set_fit(cont, LV_FIT_FILL);
    pw->ObjectPropWin()->AddObjectToWindow(cont);

    arcStartTA = PropertyControls::createNumericEntry(pw, cont, "Arc Start", "/arc/angle/start");
    arcEndTA = PropertyControls::createNumericEntry(pw, cont, "Arc End", "/arc/angle/end");

    lv_obj_t* lineProps = lv_cont_create(cont, nullptr);
    lv_cont_set_layout(lineProps, LV_LAYOUT_PRETTY);
    lv_obj_set_size(lineProps, 370, 300);
    lv_obj_t* lineLab = lv_label_create(lineProps, nullptr);
    lv_label_set_text(lineLab, "Line Properties:");
    lv_label_set_align(lineLab, LV_LABEL_ALIGN_LEFT);
    cp = new ColorPicker(0, 0, 375, 150, 0x51F542, lineProps);
    PropertyWindow::sOData odata;
    odata.pw = pw;
    odata.objName = "ArcColor";
    cp->SetCallback(PropertyControls::assignColor, odata);
    arcLineWidth = PropertyControls::createNumericEntry(pw, lineProps, "Line Width", "/arc/style/line.width");
    arcLineRound = lv_cb_create(lineProps, nullptr);
    lv_cb_set_text(arcLineRound, "Arc Line Rounded");
    sPropChange* objData = new sPropChange();
    objData->pw = pw;
    objData->propertyPath = "/arc/style/line.rounded";
    lv_obj_set_user_data(arcLineRound, static_cast<lv_obj_user_data_t>(&objData));
    lv_obj_set_event_cb(arcLineRound, PropertyControls::checkBoxCB);
}

void ArcProperties::UpdateArcProperties(PropertyWindow* pw, json j)
{
    pw->Drawing(true);
    if (pw->CurrentlyLoadedProp != PropertyWindow::ARC)
    {
        pw->ObjectPropWin()->DeleteChildren();
        CreateArcProperties(pw);
        pw->CurrentlyLoadedProp = PropertyWindow::ARC;
    }

    json ap = j["arc"];
    std::stringstream as, ae, lw;
    as << ap["angle"]["start"];
    ae << ap["angle"]["end"];
    lw << ap["style"]["line.width"];
    lv_ta_set_text(arcStartTA, as.str().c_str());
    lv_ta_set_text(arcEndTA, ae.str().c_str());
    lv_ta_set_text(arcLineWidth, lw.str().c_str());
    if (ap["style"]["line.rounded"] == 0)
        lv_cb_set_checked(arcLineRound, false);
    else
        lv_cb_set_checked(arcLineRound, true);
    pw->Drawing(false);
}
