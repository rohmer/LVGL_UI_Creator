#include <stdio.h>
#include <stdlib.h>
#include "interface.h"
#include "setting.h"
#include "toolbox.h"
#include "custom_widget.h"
#include "dataset.h"
#include "Windows/PropertyWindow.h"
#include "Windows/ToolTray.h"
#include "Windows/SimWindow.h"


lv_obj_t* screen;
lv_obj_t* tft_win;
lv_obj_t* toolbox_win;
lv_obj_t* setting_win;

void tft_win_init(lv_obj_t* parent);

void lv_gui_designer()
{
    lv_coord_t hres = lv_disp_get_hor_res(nullptr);
    lv_coord_t vres = lv_disp_get_ver_res(nullptr);

    screen = lv_obj_create(lv_disp_get_scr_act(nullptr), nullptr);
    lv_obj_set_size(screen, hres, vres);
    // lv_theme_t * th = lv_theme_material_init(150, NULL);
    // lv_theme_set_current(th);	

    //toolbox_win_init(screen); 
    //setting_win_init(screen);
    //tft_win_init(screen);
    //auto simWindow = new SimWindow(320, 240);
    //auto tb = new ToolTray(screen);
    //auto propertyWin = new PropertyWindow(simWindow);
}

void tft_win_init(lv_obj_t* parent)
{
    tft_win = lv_win_create(parent, nullptr);
    lv_coord_t win_width, win_height;
    win_width = 480;
    win_height = 320;

    char title[40];
    snprintf(title, 40, "TFT Simulator  [Size:%dx%d]", win_width, win_height);
    lv_win_set_drag(tft_win, true);
    lv_obj_set_size(tft_win, win_width, win_height);
    lv_win_set_title(tft_win, title);
    lv_obj_align(tft_win, nullptr, LV_ALIGN_CENTER, 0, 0);

    widget_set_info(tft_win, WIDGET_TYPE_OBJ); //obj

    //layerview_add(layerview_get_base(), tft_win);
}
