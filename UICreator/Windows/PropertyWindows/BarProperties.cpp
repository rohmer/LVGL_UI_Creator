#include "BarProperties.h"
lv_obj_t *BarProperties::rngStart,
         *BarProperties::rngEnd,
         *BarProperties::sym,
         *BarProperties::animTime,
         *BarProperties::value,
         *BarProperties::bg,
         *BarProperties::ind;

void BarProperties::CreateBarProperties(PropertyWindow* pw)
{
    pw->ObjectPropWin()->UpdateHeight(450);
    lv_obj_t* cont = lv_cont_create(pw->ObjectPropWin()->GetWindow(), nullptr);
    lv_cont_set_layout(cont, LV_LAYOUT_PRETTY);
    lv_cont_set_fit(cont, LV_FIT_FILL);
    pw->ObjectPropWin()->AddObjectToWindow(cont);
    lv_obj_t* props1 = lv_cont_create(cont, nullptr);
    lv_cont_set_layout(props1, LV_LAYOUT_PRETTY);
    lv_obj_set_size(props1, 370, 50);

    rngStart = PropertyControls::createNumericEntry(pw, props1, "Range Min", "/bar/range/min");
    value = PropertyControls::createNumericEntry(pw, props1, "Value", "/bar/value");
    rngEnd = PropertyControls::createNumericEntry(pw, props1, "Range Max", "/bar/range/max");
    lv_obj_t* props2 = lv_cont_create(cont, nullptr);
    lv_cont_set_layout(props2, LV_LAYOUT_PRETTY);
    lv_obj_set_size(props2, 370, 50);
    sym = PropertyControls::createCBEntry(pw, props2, "Bar Symmetrical", "/bar/sym");
    animTime = PropertyControls::createNumericEntry(pw, props2, "Animation Time", "/bar/animT");
    lv_obj_t* props3 = lv_cont_create(cont, nullptr);
    lv_cont_set_layout(props3, LV_LAYOUT_PRETTY);
    lv_obj_set_size(props3, 370, 450);
    bg = PropertyControls::createStyleEntry(pw, props3, "Style BG", "/bar/styleBG");
    ind = PropertyControls::createStyleEntry(pw, props3, "Style Indic", "/bar/styleInd");
}

void BarProperties::UpdateBarProperties(PropertyWindow* pw, json j)
{
    pw->Drawing(true);
    if (pw->CurrentlyLoadedProp != PropertyWindow::eObjType::BAR)
    {
        pw->ObjectPropWin()->DeleteChildren();
        CreateBarProperties(pw);
        pw->CurrentlyLoadedProp = PropertyWindow::eObjType::BAR;
    }

    json bp = j["bar"];
    std::stringstream minS, maxS, valueS;
    minS << j["bar"]["range"]["min"].get<unsigned>();
    maxS << j["bar"]["range"]["max"].get<unsigned>();
    valueS << j["bar"]["value"].get<unsigned>();
    lv_ta_set_text(rngStart, minS.str().c_str());
    lv_ta_set_text(rngEnd, maxS.str().c_str());
    lv_ta_set_text(value, valueS.str().c_str());
    std::stringstream animT;
    animT << j["bar"]["animT"].get<unsigned>();
    lv_ta_set_text(animTime, animT.str().c_str());

    std::string bgSty = j["bar"]["styleBG"]["name"];
    int ctr = -1;
    std::string bgOpts = lv_ddlist_get_options(bg);
    std::vector<std::string> opts = ObjectTools::Split(bgOpts, '\n');

    for (int i = 0; i < opts.size(); i++)
    {
        if (opts[i] == bgSty)
        {
            ctr = i;
            break;
        }
    }
    if (ctr != -1)
    {
        lv_ddlist_set_selected(bg, ctr);
    }

    std::string indSty = j["bar"]["styleInd"]["name"];
    ctr = -1;
    std::string indOpts = lv_ddlist_get_options(ind);
    opts = ObjectTools::Split(indOpts, '\n');

    for (int i = 0; i < opts.size(); i++)
    {
        if (opts[i] == indSty)
        {
            ctr = i;
            break;
        }
    }
    if (ctr != -1)
    {
        lv_ddlist_set_selected(ind, ctr);
    }
    pw->Drawing(false);
}
