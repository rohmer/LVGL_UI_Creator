#include "ColorPicker.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorEvents.h"

ColorPicker::ColorPicker(int x, int y, int width, int height, uint32_t startColor, lv_obj_t* parent)
{
    createObjects(parent, x, y, width, height, startColor);
}

void ColorPicker::Show()
{
    SetEditorHidden(cont, false);
}

void ColorPicker::Hide()
{
    SetEditorHidden(cont, true);
}

void ColorPicker::createObjects(
    lv_obj_t* parent,
    int x,
    int y,
    int width,
    int height,
    uint32_t startColor)
{
    if (parent == nullptr)
        parent = lv_scr_act();
    cont = lv_cont_create(parent, nullptr);
    lv_obj_set_size(cont, width, height);
    lv_obj_set_pos(cont, x, y);
    int barHeight = height / 6;
    exCol = lv_obj_create(cont, nullptr);
    lv_obj_set_pos(exCol, 5, 5);
    lv_obj_set_size(exCol, width - 10, barHeight);
#if LVGL_VERSION_MAJOR < 9
    lv_style_copy(&colStyle, &lv_style_plain);
#endif
    SetColor(startColor);

    // create the sliders
    int idx = 2;
#if LVGL_VERSION_MAJOR < 9
    static lv_style_t rStyle, bStyle, gStyle;
    lv_style_copy(&rStyle, &lv_style_pretty_color);
    lv_style_copy(&gStyle, &lv_style_pretty_color);
    lv_style_copy(&bStyle, &lv_style_pretty_color);
    rStyle.body.main_color = lv_color_make(255, 0, 0);
    rStyle.body.grad_color = lv_color_make(75, 0, 0);
    bStyle.body.main_color = lv_color_make(0, 0, 255);
    bStyle.body.grad_color = lv_color_make(0, 0, 75);
    gStyle.body.main_color = lv_color_make(0, 255, 0);
    gStyle.body.grad_color = lv_color_make(0, 75, 0);
#endif

    lv_obj_t* rLabel = lv_label_create(cont, nullptr);
    lv_label_set_text(rLabel, "Red");
    lv_obj_set_pos(rLabel, 20, barHeight * idx);
    rSlider = lv_slider_create(cont, nullptr);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_style_bg_color(rSlider, lv_color_hex(0xff0000), LV_PART_INDICATOR);
#else
    lv_obj_set_style(rSlider, &rStyle);
#endif
    lv_obj_set_size(rSlider, width - 80, barHeight - 10);
    lv_obj_set_pos(rSlider, 70, idx * barHeight - 5);
    lv_slider_set_range(rSlider, 0, 255);
    lv_slider_set_value(rSlider, (startColor >> 16) & 255, LV_ANIM_OFF);
    rCBD = new sliderCBData();
    rCBD->channel = R;
    rCBD->cp = this;
    lv_obj_set_user_data(rSlider, static_cast<lv_obj_user_data_t>(rCBD));
    SetEditorEventCallback<sliderCB>(rSlider);
    idx++;
    lv_obj_t* gLabel = lv_label_create(cont, nullptr);
    lv_label_set_text(gLabel, "Green");
    lv_obj_set_pos(gLabel, 10, barHeight * idx);
    gSlider = lv_slider_create(cont, nullptr);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_style_bg_color(gSlider, lv_color_hex(0x00a000), LV_PART_INDICATOR);
#else
    lv_obj_set_style(gSlider, &gStyle);
#endif
    lv_obj_set_size(gSlider, width - 80, barHeight - 10);
    lv_obj_set_pos(gSlider, 70, idx * barHeight - 5);
    lv_slider_set_range(gSlider, 0, 255);
    lv_slider_set_value(gSlider, (startColor >> 8) & 255, LV_ANIM_OFF);
    gCBD = new sliderCBData();
    gCBD->channel = G;
    gCBD->cp = this;
    lv_obj_set_user_data(gSlider, static_cast<lv_obj_user_data_t>(gCBD));
    SetEditorEventCallback<sliderCB>(gSlider);
    idx++;
    lv_obj_t* bLabel = lv_label_create(cont, nullptr);
    lv_label_set_text(bLabel, "Blue");
    lv_obj_set_pos(bLabel, 15, barHeight * idx);
    bSlider = lv_slider_create(cont, nullptr);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_style_bg_color(bSlider, lv_color_hex(0x0000ff), LV_PART_INDICATOR);
#else
    lv_obj_set_style(bSlider, &bStyle);
#endif
    lv_obj_set_size(bSlider, width - 80, barHeight - 10);
    lv_obj_set_pos(bSlider, 70, idx * barHeight - 5);
    lv_slider_set_range(bSlider, 0, 255);
    lv_slider_set_value(bSlider, startColor & 255, LV_ANIM_OFF);
    bCBD = new sliderCBData();
    bCBD->channel = B;
    bCBD->cp = this;
    lv_obj_set_user_data(bSlider, static_cast<lv_obj_user_data_t>(bCBD));
    SetEditorEventCallback<sliderCB>(bSlider);
    idx++;
    lv_obj_t* applyButton = lv_btn_create(cont, nullptr);
    lv_obj_t* applyLabel = lv_label_create(applyButton, nullptr);
    lv_label_set_text(applyLabel, "Apply Color");
    lv_obj_set_user_data(applyButton, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<applyCB>(applyButton);
    lv_obj_set_size(applyButton, width - 20, barHeight - 10);
    lv_obj_set_pos(applyButton, 10, idx * barHeight);
}

void ColorPicker::SetCallback(cp_callback cb, std::any callbackObject)
{
    setCB = cb;
    this->cbObject = callbackObject;
}

void ColorPicker::applyCB(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    ColorPicker* cp = static_cast<ColorPicker*>(lv_obj_get_user_data(obj));
    lv_color_t col = lv_color_make(
        lv_slider_get_value(cp->rSlider),
        lv_slider_get_value(cp->gSlider),
        lv_slider_get_value(cp->bSlider)
    );
    if (cp->setCB != nullptr)
    {
        cp->setCB(col, cp->cbObject);
    }
}

void ColorPicker::sliderCB(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_VALUE_CHANGED)
        return;
    sliderCBData* data = static_cast<sliderCBData*>(lv_obj_get_user_data(obj));
    int red = lv_slider_get_value(data->cp->rSlider);
    int green = lv_slider_get_value(data->cp->gSlider);
    int blue = lv_slider_get_value(data->cp->bSlider);
    data->cp->SetColor(red, green, blue, 255);
}

void ColorPicker::SetColor(uint32_t color)
{
    SetColor((color >> 16) & 255, (color >> 8) & 255, color & 255, 255);
}

void ColorPicker::SetColor(int R, int G, int B, int A)
{
    lv_color_t col = lv_color_make(R, G, B);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_style_bg_color(exCol, col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(exCol, A, LV_PART_MAIN);
#else
    col.ch.alpha = A;
    colStyle.body.main_color = col;
    colStyle.body.grad_color = col;
    lv_obj_set_style(exCol, &colStyle);
#endif
}
