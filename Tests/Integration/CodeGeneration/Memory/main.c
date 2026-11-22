#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef UI_MEMORY_PRIVATE_WIDGETS
#include <src/widgets/label/lv_label_private.h>
#include <src/widgets/checkbox/lv_checkbox_private.h>
#include <src/widgets/dropdown/lv_dropdown_private.h>
#endif

#define CHECK(value) do { if (!(value)) { fprintf(stderr, "Failed %d: %s\n", __LINE__, #value); return 1; } } while (0)
#define OBJECT(id) ui_get_object(0, id)

#if LVGL_VERSION_MAJOR >= 9
static void Flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area; (void)pixels; lv_display_flush_ready(display);
}
#else
static void Flush(lv_disp_drv_t *display, const lv_area_t *area, lv_color_t *pixels)
{
    (void)area; (void)pixels; lv_disp_flush_ready(display);
}
#endif

static size_t LiteralHeapBytes(void)
{
    size_t bytes = 0;
    if (!((lv_label_t *)OBJECT(1))->static_txt) bytes += strlen(lv_label_get_text(OBJECT(1))) + 1;
    if (!((lv_checkbox_t *)OBJECT(2))->static_txt) bytes += strlen(lv_checkbox_get_text(OBJECT(2))) + 1;
#if LVGL_VERSION_MAJOR > 9 || (LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 5)
    /* The supported LVGL 9.5+ headers renamed the options-ownership bit;
     * 9.1-9.3 and 8.3 use static_txt for the same immutable-options check. */
    if (!((lv_dropdown_t *)OBJECT(3))->static_options) bytes += strlen(lv_dropdown_get_options(OBJECT(3))) + 1;
#else
    if (!((lv_dropdown_t *)OBJECT(3))->static_txt) bytes += strlen(lv_dropdown_get_options(OBJECT(3))) + 1;
#endif
    return bytes;
}

int main(void)
{
    unsigned iteration;
    size_t literal_heap_bytes = 0;
    void *pixels = malloc(320 * 40 * 4);
    CHECK(pixels);
    lv_init();
#if LVGL_VERSION_MAJOR >= 9
    lv_display_t *display = lv_display_create(320, 240);
    lv_display_set_buffers(display, pixels, NULL, 320 * 40 * 4, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, Flush);
#else
    lv_disp_draw_buf_t buffer;
    lv_disp_drv_t driver;
    lv_disp_draw_buf_init(&buffer, pixels, NULL, 320 * 40);
    lv_disp_drv_init(&driver);
    driver.hor_res = 320; driver.ver_res = 240;
    driver.draw_buf = &buffer; driver.flush_cb = Flush;
    lv_disp_t *display = lv_disp_drv_register(&driver);
#endif
    for (iteration = 0; iteration < 3; ++iteration) {
        const lv_image_dsc_t *first;
        const lv_image_dsc_t *second;
        const uint8_t *canvas_first;
        const uint8_t *canvas_second;
        char edited[] = "Application-owned mutable update";
        CHECK(ui_init());
        CHECK(strcmp(lv_label_get_text(OBJECT(1)), "Persistent label text remains in read-only generated storage.") == 0);
        CHECK(strcmp(lv_checkbox_get_text(OBJECT(2)), "Persistent checkbox caption also stays in read-only storage.") == 0);
        CHECK(lv_dropdown_get_selected(OBJECT(3)) == 1);
        literal_heap_bytes = LiteralHeapBytes();
#if UI_MEMORY_EXPECT_STATIC_TEXT && !LV_USE_ARABIC_PERSIAN_CHARS
        CHECK(literal_heap_bytes == 0);
#else
        CHECK(literal_heap_bytes == 196);
        /* Disabling immutable text supports direct LVGL editing and mode
         * changes on the original authored label, without replacing its text. */
        lv_label_ins_text(OBJECT(1), 0, "Edit: ");
        CHECK(strncmp(lv_label_get_text(OBJECT(1)), "Edit: Persistent", 16) == 0);
        lv_label_cut_text(OBJECT(1), 0, 6);
        CHECK(strcmp(lv_label_get_text(OBJECT(1)), "Persistent label text remains in read-only generated storage.") == 0);
        lv_label_set_long_mode(OBJECT(1), LV_LABEL_LONG_MODE_DOTS);
        CHECK(lv_label_get_long_mode(OBJECT(1)) == LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_size(OBJECT(1), 40, 20);
        lv_obj_update_layout(OBJECT(1));
        CHECK(!((lv_label_t *)OBJECT(1))->static_txt);
        lv_label_set_long_mode(OBJECT(1), LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_size(OBJECT(1), 320, 24);
#endif
        CHECK(!((lv_label_t *)OBJECT(4))->static_txt);
        first = (const lv_image_dsc_t *)lv_image_get_src(OBJECT(5));
        second = (const lv_image_dsc_t *)lv_image_get_src(OBJECT(6));
        CHECK(first && second && first->data_size == 16 && second->data_size == 16);
        CHECK(memcmp(first->data, second->data, 16) == 0);
#if UI_MEMORY_EXPECT_OPTIMIZED
        CHECK(first->data == second->data);
#endif
#if LVGL_VERSION_MAJOR >= 9
        canvas_first = lv_canvas_get_image(OBJECT(7))->data;
        canvas_second = lv_canvas_get_image(OBJECT(8))->data;
#else
        canvas_first = lv_canvas_get_img(OBJECT(7))->data;
        canvas_second = lv_canvas_get_img(OBJECT(8))->data;
#endif
        CHECK(canvas_first != canvas_second);
        {
            const uint8_t before = canvas_second[0];
            ((uint8_t *)canvas_first)[0] ^= 0xff;
            CHECK(canvas_second[0] == before);
        }
        lv_label_set_text(OBJECT(1), edited);
        lv_checkbox_set_text(OBJECT(2), edited);
        lv_dropdown_set_options(OBJECT(3), edited);
        memset(edited, '?', strlen(edited));
        CHECK(strcmp(lv_label_get_text(OBJECT(1)), "Application-owned mutable update") == 0);
        CHECK(strcmp(lv_checkbox_get_text(OBJECT(2)), "Application-owned mutable update") == 0);
        CHECK(strcmp(lv_dropdown_get_options(OBJECT(3)), "Application-owned mutable update") == 0);
        CHECK(!((lv_label_t *)OBJECT(1))->static_txt);
        lv_obj_update_layout(OBJECT(4));
        ui_deinit();
        CHECK(OBJECT(1) == NULL);
    }
    lv_display_delete(display);
#if LVGL_VERSION_MAJOR >= 9
    lv_deinit();
#endif
    free(pixels);
    printf("Generated immutable text heap payload: %zu bytes; deduplication, writable canvases, dots and lifecycle passed\n", literal_heap_bytes);
    return 0;
}
