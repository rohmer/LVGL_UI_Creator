#include "BarProperties.h"
lv_obj_t* BarProperties::rngStart,
* BarProperties::rngEnd,
* BarProperties::sym,
* BarProperties::animTime;

void BarProperties::CreateBarProperties(PropertyWindow* pw)
{
    pw->ObjectPropWin()->UpdateHeight(300);
    lv_obj_t* cont = lv_cont_create(pw->ObjectPropWin()->GetWindow(), nullptr);
    lv_cont_set_layout(cont, LV_LAYOUT_PRETTY);
    lv_cont_set_fit(cont, LV_FIT_FILL);

    rngStart= PropertyControls::createNumericEntry(pw, cont, "Range Min", "/bar/range/min");
    rngEnd = PropertyControls::createNumericEntry(pw, cont, "Range Max", "/bar/range/max");
    sym = PropertyControls::createCBEntry(pw, cont, "Bar Symmetrical", "/bar/sym");
    animTime = PropertyControls::createNumericEntry(pw, cont, "Animation Time", "/bar/animT");
    PropertyControls::createStyleEntry(pw, cont, "Style BG", "/bar/styleBG");
    PropertyControls::createStyleEntry(pw, cont, "Style Indic", "/bar/styleInd");
}

void BarProperties::UpdateBarProperties(PropertyWindow* pw, json j)
{
    pw->Drawing(true);
    if (pw->CurrentlyLoadedProp != PropertyWindow::eObjType::BAR)
    {
        pw->ObjectPropWin()->DeleteChildren();
        CreateBarProperties(pw);

    }

}