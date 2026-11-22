#ifndef LVGL_PICO_H
#define LVGL_PICO_H
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct lvgl_pico_display lvgl_pico_display_t;
/* A synchronous driver calls lvgl_pico_flush_ready before returning. An
 * asynchronous/DMA driver calls it once after the transfer has completed. */
typedef void (*lvgl_pico_flush_cb_t)(lvgl_pico_display_t* port, const lv_area_t* area,
    const uint8_t* pixels, uint32_t stride, void* context);
typedef void (*lvgl_pico_touch_cb_t)(lv_indev_data_t* data, void* context);

struct lvgl_pico_display {
    lv_display_t* display;
    lv_indev_t* pointer;
    lvgl_pico_flush_cb_t flush;
    lvgl_pico_touch_cb_t touch;
    void* context;
};
typedef struct {
    int32_t width, height;
    lv_color_format_t color_format;
    void* draw_buffer;
    uint32_t draw_buffer_bytes;
    lvgl_pico_flush_cb_t flush;
    lvgl_pico_touch_cb_t touch; /* Optional; omit for a display without touch. */
    void* context;
} lvgl_pico_display_config_t;

/* Call after lv_init. Uses the Pico SDK monotonic timer, without a timer task. */
void lvgl_pico_configure_tick(void);
/* The caller zero-initializes port and owns its draw buffer. All operations run
 * on one LVGL owner core/task. Display and input objects are the only allocations
 * made here; no additional framebuffer or full-screen buffer is allocated. */
bool lvgl_pico_display_init(lvgl_pico_display_t* port, const lvgl_pico_display_config_t* config);
/* Destroy the loaded runtime/generated UI and finish DMA transfers first. */
void lvgl_pico_display_deinit(lvgl_pico_display_t* port);
void lvgl_pico_flush_ready(lvgl_pico_display_t* port);

#ifdef __cplusplus
}
#endif
#endif
