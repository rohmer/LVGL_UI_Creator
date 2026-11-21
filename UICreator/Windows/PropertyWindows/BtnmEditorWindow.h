#pragma once
#include <any>
#include "../../Utils/EditorEvents.h"
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
        unsigned int width;
        uint16_t ctrlBits = 0;
    };

private:
    static lv_obj_t* window, * preview, * previewCont, * recolor, * oneToggle, * allCont,
        * width, * height, *buttonWindow;
    static lv_obj_t* addLeft, * addRight, * addAbove, * addBelow, * btnLabel, *delBtn, *btnWidth;
    static lv_obj_t* hidden, * noRepeat, * inactive, * tglEnable, * tglState, * clickTrig;

    static int listContWidth, listContHeight;
    static PropertyWindow* pw;

    static void closeBtnCB(lv_obj_t* obj, EditorEventCode event);
    static void previewCB(lv_obj_t* obj, EditorEventCode event);
    static void recolorCB(lv_obj_t* obj, EditorEventCode event);
    static void oneToggleCB(lv_obj_t* obj, EditorEventCode event);
    static void taCB(lv_obj_t* obj, EditorEventCode event);
    static void btnDel(lv_obj_t* obj, EditorEventCode event);

    static void addLeftCB(lv_obj_t* obj, EditorEventCode event);
    static void addAboveCB(lv_obj_t* obj, EditorEventCode event);
    static void addRightCB(lv_obj_t* obj, EditorEventCode event);
    static void addBelowCB(lv_obj_t* obj, EditorEventCode event);

    static void optCB(lv_obj_t* obj, EditorEventCode event);

    static void resize();    
    static void loadButtons();

    static std::vector<btnStruct> buttons;

    static uint8_t get_button_width(uint16_t ctrl_bits);
    static bool buttonEdDrawn;

    static void updateButtonEd(int buttonNum);
    static void createButtonEd();
    static void resetBtnNums();

    static int selectedButton;
    static bool changingSel;
};
