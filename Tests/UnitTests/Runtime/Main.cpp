#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <lvgl.h>
#if __has_include(<RuntimeCompat.h>)
#include <RuntimeCompat.h>
#endif
#if LVGL_VERSION_MAJOR == 8
static void Flush(lv_disp_drv_t* driver,const lv_area_t*,lv_color_t*){lv_disp_flush_ready(driver);}
#endif
int main(int argc, char **argv)
{
    lv_init();
#if LVGL_VERSION_MAJOR == 8
    static lv_color_t pixels[800*16];static lv_disp_draw_buf_t draw;
    lv_disp_draw_buf_init(&draw,pixels,nullptr,800*16);
    static lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=800;driver.ver_res=600;driver.draw_buf=&draw;driver.flush_cb=Flush;
    if(!lv_disp_drv_register(&driver))
#else
    if (!lv_display_create(800, 600))
#endif
        return 1;
    int result = Catch::Session().run(argc, argv);
#if LVGL_VERSION_MAJOR >= 9
    lv_deinit();
#endif
    return result;
}
