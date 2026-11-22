#include "lvgl_pico.h"
#include "pico/time.h"
#include <string.h>

static uint32_t milliseconds(void) { return (uint32_t)(time_us_64() / 1000u); }
void lvgl_pico_configure_tick(void) { lv_tick_set_cb(milliseconds); }

static void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels)
{
    lvgl_pico_display_t* port = (lvgl_pico_display_t*)lv_display_get_user_data(display);
    uint32_t stride = lv_draw_buf_width_to_stride((uint32_t)lv_area_get_width(area), lv_display_get_color_format(display));
    port->flush(port, area, pixels, stride, port->context);
}
static void read_touch(lv_indev_t* input, lv_indev_data_t* data)
{
    lvgl_pico_display_t* port = (lvgl_pico_display_t*)lv_indev_get_user_data(input);
    data->state = LV_INDEV_STATE_RELEASED;
    port->touch(data, port->context);
}
bool lvgl_pico_display_init(lvgl_pico_display_t* port, const lvgl_pico_display_config_t* config)
{
    if (!port || !config || port->display || config->width < 1 || config->height < 1 ||
        config->width > 8192 || config->height > 8192 ||
        !config->draw_buffer || !config->draw_buffer_bytes || !config->flush ||
        (config->color_format != LV_COLOR_FORMAT_RGB565 && config->color_format != LV_COLOR_FORMAT_ARGB8888)) return false;
    if (config->draw_buffer_bytes < lv_draw_buf_width_to_stride((uint32_t)config->width, config->color_format)) return false;
    port->flush = config->flush; port->touch = config->touch; port->context = config->context;
    port->display = lv_display_create(config->width, config->height);
    if (!port->display) return false;
    lv_display_set_color_format(port->display, config->color_format);
    lv_display_set_buffers(port->display, config->draw_buffer, NULL, config->draw_buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(port->display, port);
    lv_display_set_flush_cb(port->display, flush);
    if (port->touch) {
        port->pointer = lv_indev_create();
        if (!port->pointer) { lvgl_pico_display_deinit(port); return false; }
        lv_indev_set_type(port->pointer, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(port->pointer, port->display);
        lv_indev_set_user_data(port->pointer, port);
        lv_indev_set_read_cb(port->pointer, read_touch);
    }
    return true;
}
void lvgl_pico_display_deinit(lvgl_pico_display_t* port)
{
    if (!port) return;
    if (port->pointer) lv_indev_delete(port->pointer);
    if (port->display) lv_display_delete(port->display);
    memset(port, 0, sizeof *port);
}
void lvgl_pico_flush_ready(lvgl_pico_display_t* port)
{
    if (port && port->display) lv_display_flush_ready(port->display);
}
