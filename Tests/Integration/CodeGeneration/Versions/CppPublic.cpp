#include "ui.h"
#include <cstdio>
#include <cstring>
int main()
{
    lv_init();
#if LVGL_VERSION_MAJOR >= 9
    auto *display = lv_display_create(800, 600);
#else
    lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = 800;
    driver.ver_res = 600;
    auto *display = lv_disp_drv_register(&driver);
#endif
    if (!ui_init())
        return 1;
#ifdef TEST_assets
    if (!ui_get_font("font_1") || !ui_get_image("image_1"))
        return 2;
    if (lv_image_get_src(ui_get_object(0, 2)) != ui_get_image("image_1"))
        return 3;
#else
    if (!ui_set_v_caption("C++ firmware") || std::strcmp(ui_variables.v_caption, "C++ firmware"))
        return 4;
#endif
    ui_deinit();
#if LVGL_VERSION_MAJOR >= 9
    lv_display_delete(display);
    lv_deinit();
#else
    lv_disp_remove(display);
#endif
    std::puts("C++ application integration passed");
    return 0;
}
