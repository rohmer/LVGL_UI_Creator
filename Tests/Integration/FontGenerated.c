#include "converted_font.h"
#include "converted_kern_font.h"
#include <stdio.h>
int main(void)
{
    lv_font_glyph_dsc_t glyph = {0};
    lv_init();
    if (!lv_font_get_glyph_dsc(&converted_font, &glyph, 'A', 'V') || !glyph.adv_w ||
        converted_font.line_height <= 0)
        return 1;
    lv_draw_buf_t *buffer = lv_draw_buf_create(glyph.box_w, glyph.box_h, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
    if (!buffer)
        return 2;
    if (!lv_font_get_glyph_bitmap(&glyph, buffer))
        return 3;
    lv_font_glyph_release_draw_data(&glyph);
    lv_draw_buf_destroy(buffer);
    lv_font_glyph_dsc_t unkerned = {0}, kerned = {0};
    if (!lv_font_get_glyph_dsc(&converted_kern_font, &unkerned, 'A', 0) ||
        !lv_font_get_glyph_dsc(&converted_kern_font, &kerned, 'A', 'V') ||
        kerned.adv_w >= unkerned.adv_w)
        return 4;
    buffer = lv_draw_buf_create(kerned.box_w, kerned.box_h, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
    if (!buffer || !lv_font_get_glyph_bitmap(&kerned, buffer))
        return 5;
    lv_font_glyph_release_draw_data(&kerned);
    lv_draw_buf_destroy(buffer);
    lv_deinit();
    puts("Converted C font compiled and rendered glyphs");
    return 0;
}
