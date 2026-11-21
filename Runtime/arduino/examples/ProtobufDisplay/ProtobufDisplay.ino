#include <Arduino.h>
#include <lvgl_pb_arduino.h>
#include <lvgl_pb_project_config.h>

// Match the ESP-IDF sample's stack budget for document loading and LVGL events.
SET_LOOP_TASK_STACK_SIZE(16384);

// Nano ESP32 has no built-in screen. Replace these two functions with your
// display/touch driver's operations. This example uses synchronous transfers.
#if LVGL_PB_COLOR_DEPTH == 16
using Pixel = uint16_t;
static constexpr auto pixel_format=LV_COLOR_FORMAT_RGB565;
#else
using Pixel = uint32_t;
static constexpr auto pixel_format=LV_COLOR_FORMAT_ARGB8888;
#endif
static void board_write_pixels(int32_t x,int32_t y,int32_t width,int32_t height,const uint8_t* pixels)
{
    (void)x;(void)y;(void)width;(void)height;(void)pixels;
}
static bool board_read_touch(int32_t* x,int32_t* y)
{
    (void)x;(void)y;return false;
}
static lvgl_runtime_t* project=nullptr;
static Pixel draw_buffer[LVGL_PB_DEFAULT_WIDTH*20];
static void flush(lv_display_t* display,const lv_area_t* area,uint8_t* pixels)
{
    board_write_pixels(area->x1,area->y1,lv_area_get_width(area),lv_area_get_height(area),pixels);
    lv_display_flush_ready(display);
}
static void touch(lv_indev_t*,lv_indev_data_t* data)
{
    int32_t x=0,y=0;
    data->state=board_read_touch(&x,&y)?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
    data->point={x,y};
}
void setup()
{
    Serial.begin(115200);
    lv_init();lvgl_pb_arduino_configure_tick();
    auto* display=lv_display_create(LVGL_PB_DEFAULT_WIDTH,LVGL_PB_DEFAULT_HEIGHT);
    lv_display_set_color_format(display,pixel_format);
    lv_display_set_buffers(display,draw_buffer,nullptr,sizeof draw_buffer,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    auto* input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_display(input,display);lv_indev_set_read_cb(input,touch);
    char error[192];
    lvgl_runtime_options_t options={display,16u*1024u*1024u,true};
    if (!lvgl_pb_embedded_project_count || !lvgl_pb_arduino_load_embedded(lvgl_pb_embedded_projects[0].name,&options,&project,error,sizeof error)) {
        Serial.println(lvgl_pb_embedded_project_count?error:"No embedded Protobuf projects");
    }
}
void loop()
{
    const auto wait=lv_timer_handler();
    delay(wait>20?20:(wait<1?1:wait));
}
