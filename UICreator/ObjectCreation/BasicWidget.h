#pragma once
#include <Serialization/BasicWidgets.h>

namespace BasicWidget
{
#if LVGL_VERSION_MAJOR >= 9
    inline lv_obj_t* Create(int id, lv_obj_t* parent, int x, int y, std::string& name)
    {
        lv_obj_t* object = nullptr;
        switch (id)
        {
        case 6: name = "Check box"; object = lv_checkbox_create(parent); lv_checkbox_set_text(object, "Check box"); break;
        case 8: name = "Container"; object = lv_obj_create(parent); lv_obj_set_size(object, 140, 90); break;
        case 9: name = "Drop down list"; object = lv_dropdown_create(parent); lv_dropdown_set_options(object, "First\nSecond\nThird"); break;
        case 11: name = "Image"; object = lv_image_create(parent); lv_image_set_src(object, LV_SYMBOL_IMAGE); break;
        case 14: name = "Label"; object = lv_label_create(parent); lv_label_set_text(object, "Label"); break;
        case 15: name = "LED"; object = lv_led_create(parent); lv_led_set_color(object, lv_color_hex(0x3399ff)); break;
        case 16:
            name = "Line"; object = lv_line_create(parent);
            Serialization::BasicWidgets::Apply(object, {{"points", {{0, 0}, {100, 40}}}, {"invertY", false}});
            lv_obj_set_style_line_width(object, 3, LV_PART_MAIN);
            break;
        case 20: name = "Preloader"; object = lv_spinner_create(parent); lv_obj_set_size(object, 60, 60); break;
        case 21: name = "Roller"; object = lv_roller_create(parent); lv_roller_set_options(object, "First\nSecond\nThird", LV_ROLLER_MODE_NORMAL); break;
        case 22: name = "Slider"; object = lv_slider_create(parent); lv_obj_set_size(object, 140, 16); lv_slider_set_value(object, 50, LV_ANIM_OFF); break;
        case 23: name = "Spinbox"; object = lv_spinbox_create(parent); lv_obj_set_width(object, 120); break;
        case 24: name = "Switch"; object = lv_switch_create(parent); break;
        case 27: name = "Text area"; object = lv_textarea_create(parent); lv_obj_set_size(object, 160, 70); lv_textarea_set_text(object, "Text"); break;
        default: return nullptr;
        }
        lv_obj_set_pos(object, x, y);
        lv_obj_set_clickable(object, true);
        return object;
    }
#endif
}
