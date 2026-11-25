#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TEST_main
#include "fixture_ids.h"
#define OBJ(name) ui_get_object(0, ID_##name)
#endif
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                                \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
#ifdef TEST_main
#include "../InputModes.h"
#endif
#if LVGL_VERSION_MAJOR >= 9
static void Flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}
#else
static void Flush(lv_disp_drv_t *display, const lv_area_t *area, lv_color_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_disp_flush_ready(display);
}
#endif
int main(void)
{
    lv_obj_t *previous;
    int id, iteration;
    void *pixels = malloc(800 * 40 * 4);
    lv_init();
#if LVGL_VERSION_MAJOR >= 9
    lv_display_t *display = lv_display_create(800, 600);
    lv_display_set_buffers(display, pixels, NULL, 800 * 40 * 4, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, Flush);
#else
    lv_disp_draw_buf_t buffer;
    lv_disp_drv_t driver;
    lv_disp_draw_buf_init(&buffer, pixels, NULL, 800 * 40);
    lv_disp_drv_init(&driver);
    driver.hor_res = 800;
    driver.ver_res = 600;
    driver.draw_buf = &buffer;
    driver.flush_cb = Flush;
    lv_disp_t *display = lv_disp_drv_register(&driver);
#endif
    previous = lv_screen_active();
    for (iteration = 0; iteration < 3; ++iteration)
    {
        CHECK(ui_init());
        CHECK(ui_init());
#ifdef TEST_main
        CHECK(VerifyInputModes() == 0);
        CHECK(ui_screen_count() == 2);
        for (id = 1; id <= 31; ++id)
            CHECK(ui_get_object(0, id));
        CHECK(ui_get_object(0, 100));
        CHECK(lv_arc_get_value(OBJ(arc)) == 42);
        CHECK(lv_bar_get_value(OBJ(bar)) == 51);
        CHECK(lv_slider_get_value(OBJ(slider)) == 75);
        CHECK(lv_spinbox_get_value(OBJ(spinbox)) == 123);
        CHECK(lv_dropdown_get_selected(OBJ(dropdown)) == 2);
        CHECK(lv_roller_get_selected(OBJ(roller)) == 1);
        CHECK(strcmp(lv_table_get_cell_value(OBJ(table), 1, 1), "Cell") == 0);
        CHECK(ui_set_v_level(70));
        CHECK(lv_bar_get_value(OBJ(bar)) == 70);
        lv_bar_set_value(OBJ(bar), 22, LV_ANIM_OFF);
        lv_obj_send_event(OBJ(bar), LV_EVENT_VALUE_CHANGED, NULL);
        CHECK(ui_variables.v_level == 22);
        CHECK(ui_set_v_caption("Device text"));
        CHECK(strcmp(lv_textarea_get_text(OBJ(textarea)), "Device text") == 0);
        lv_textarea_set_text(OBJ(textarea), "Typed");
        CHECK(strcmp(ui_variables.v_caption, "Typed") == 0);
        CHECK(ui_set_v_enabled(false));
        CHECK(!lv_obj_is_checked(OBJ(switch)));
        CHECK(ui_set_v_fraction(0.5));
        CHECK(ui_set_v_timestamp(INT64_C(1770000001)));
        CHECK(ui_set_v_custom_level(80));
        CHECK(ui_set_v_custom_selected(1));
        CHECK(ui_show_screen(1));
        CHECK(!ui_show_screen(99));
#else
        CHECK(ui_screen_count() == 1);
        for (id = 0; id < 8; ++id)
            CHECK(ui_get_object(0, id));
        CHECK(ui_get_font("font_1"));
        CHECK(lv_obj_get_style_text_font(ui_get_object(0, 0), 0) == ui_get_font("font_1"));
        CHECK(lv_obj_get_style_text_font(ui_get_object(0, 1), 0) == ui_get_font("font_1"));
        CHECK(ui_get_font("font_alias") == ui_get_font("font_1"));
        CHECK(ui_get_font(NULL) == NULL && ui_get_font("missing") == NULL);
        CHECK(ui_get_font("font_unused") == NULL);
        {
            const lv_font_t *font = ui_get_font("font_compressed");
            lv_font_glyph_dsc_t glyph;
            CHECK(font);
            lv_obj_add_state(ui_get_object(0, 0), LV_STATE_PRESSED);
            CHECK(lv_obj_get_style_text_font(ui_get_object(0, 0), 0) == font);
            CHECK(lv_font_get_glyph_dsc(font, &glyph, 'A', 0));
            CHECK(glyph.box_w == 1 && glyph.box_h == 1 && glyph.adv_w == 1);
#if LVGL_VERSION_MAJOR == 8
            {const uint8_t *bitmap = lv_font_get_glyph_bitmap(font, 'A');CHECK(bitmap && (bitmap[0] & 0xf0) == 0xf0);}
#else
            {
                lv_draw_buf_t *bitmap = lv_draw_buf_create(1, 1, LV_COLOR_FORMAT_A8, 0);
                CHECK(bitmap);
#if LVGL_VERSION_MINOR == 1
                CHECK(lv_font_get_glyph_bitmap(&glyph, 'A', bitmap));
#else
                CHECK(lv_font_get_glyph_bitmap(&glyph, bitmap));
#endif
                CHECK(bitmap->data[0] == 255);
#if LVGL_VERSION_MINOR >= 3
                lv_font_glyph_release_draw_data(&glyph);
#endif
                lv_draw_buf_destroy(bitmap);
            }
#endif
            lv_obj_remove_state(ui_get_object(0, 0), LV_STATE_PRESSED);
        }
        CHECK(ui_get_image("image_1"));
        CHECK(lv_image_get_src(ui_get_object(0, 2)) == ui_get_image("image_1"));
        CHECK(lv_image_get_src(ui_get_object(0, 6)) == ui_get_image("image_1"));
        CHECK(lv_imagebutton_get_src_middle(ui_get_object(0, 7), 1) == ui_get_image("image_1"));
        {
            lv_font_glyph_dsc_t glyph;
            CHECK(lv_font_get_glyph_dsc(ui_get_font("font_1"), &glyph, 'A', 0));
            CHECK(glyph.adv_w > 0);
        }
#endif
        lv_tick_inc(150);
        lv_timer_handler();
        ui_deinit();
        CHECK(lv_screen_active() == previous);
        CHECK(ui_get_screen(0) == NULL);
    }
#if LVGL_VERSION_MAJOR >= 9
    lv_display_delete(display);
#else
    lv_disp_remove(display);
#endif
#if LVGL_VERSION_MAJOR >= 9
    lv_deinit();
#endif
    free(pixels);
    puts("Versioned generated project passed");
    return 0;
}
