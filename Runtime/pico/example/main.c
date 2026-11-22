/* Supply the physical display and optional touch transport for your hardware.
 * The default callbacks intentionally discard pixels and report no touches, so
 * this UF2 can validate loading/rendering without assuming a panel or pinout. */
#include "pico/stdlib.h"
#include "lvgl_pico.h"
#include "lvgl_pico_project_config.h"
#include <stdio.h>
#if LVGL_PICO_C_PROJECT
#include "ui.h"
#else
#include "lvgl_pb_pico.h"
#endif

static _Alignas(16) uint8_t draw_buffer[LVGL_PICO_DRAW_BUFFER_BYTES];
static lvgl_pico_display_t port;

static void board_write_pixels(lvgl_pico_display_t* display, const lv_area_t* area,
    const uint8_t* pixels, uint32_t stride, void* context)
{
    (void)area; (void)pixels; (void)stride; (void)context;
    /* Transfer RGB565/ARGB8888 pixels through your SPI/parallel display driver.
     * For DMA call flush_ready only after completion, not immediately here. */
    lvgl_pico_flush_ready(display);
}
int main(void)
{
    stdio_init_all(); lv_init(); lvgl_pico_configure_tick();
    const lvgl_pico_display_config_t config = {
        .width = LVGL_PICO_WIDTH, .height = LVGL_PICO_HEIGHT,
        .color_format = LV_COLOR_FORMAT_DEFAULT, .draw_buffer = draw_buffer,
        .draw_buffer_bytes = sizeof draw_buffer, .flush = board_write_pixels,
        .touch = NULL, .context = NULL
    };
    if (!lvgl_pico_display_init(&port, &config)) panic("Cannot create LVGL Pico display");
#if LVGL_PICO_C_PROJECT
    if (!ui_init()) panic("Cannot create generated LVGL UI");
#else
    lvgl_runtime_t* project = NULL; char error[192];
    const lvgl_runtime_options_t options = {.display = port.display, .max_input_bytes = 0, .load_initial_screen = true};
    if (!lvgl_pb_pico_load_embedded(LVGL_PICO_PROJECT_NAME, &options, &project, error, sizeof error))
        panic("Cannot load Protobuf UI: %s", error);
#endif
    for (;;) {
        uint32_t wait = lv_timer_handler();
        if (wait < 1) wait = 1; if (wait > 20) wait = 20;
        sleep_ms(wait);
    }
}
