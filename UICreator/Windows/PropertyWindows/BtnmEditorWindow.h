#pragma once
#include <any>
#include <string>
#include <map>
#include <vector>
#include "lvgl/lvgl.h"
#include "../PropertyWindow.h"

class BtnmEditorWindow
{
public:
    static void CreateBtnmEditorWindow(PropertyWindow* pw);
    struct btnStruct
    {
        std::string btnName;
        int btnNum, lineNum;
        std::string tn, n;
        unsigned int width;
        uint16_t ctrlBits;
    };

private:
    static lv_obj_t* window, * preview, * previewCont, * recolor, * oneToggle, * allCont,
        * width, * height, *buttonWindow;
    static lv_obj_t* addLeft, * addRight, * addAbove, * addBelow, * btnLabel, *delBtn, *btnWidth;

    static int listContWidth, listContHeight;
    static PropertyWindow* pw;

    static void closeBtnCB(lv_obj_t* obj, lv_event_t event);
    static void previewCB(lv_obj_t* obj, lv_event_t event);
    static void recolorCB(lv_obj_t* obj, lv_event_t event);
    static void oneToggleCB(lv_obj_t* obj, lv_event_t event);
    static void taCB(lv_obj_t* obj, lv_event_t event);
    static void btnDel(lv_obj_t* obj, lv_event_t event);

    static void resize();    
    static void loadButtons();

    static std::vector<btnStruct> buttons;

    static uint8_t get_button_width(lv_btnm_ctrl_t ctrl_bits);
    static bool buttonEdDrawn;

    static void updateButtonEd(int buttonNum);
    static void createButtonEd();

};