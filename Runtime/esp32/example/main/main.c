/* SPI ST7789 example. Set pins, panel orientation and resolution for your board.
 * DMA buffers remain internal/DMA-capable even when ordinary heap uses PSRAM. */
#include "sdkconfig.h"
#include "lvgl_pb_esp32.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

#ifndef CONFIG_EXAMPLE_LCD_SWAP_XY
#define CONFIG_EXAMPLE_LCD_SWAP_XY 0
#endif
#ifndef CONFIG_EXAMPLE_LCD_INVERT
#define CONFIG_EXAMPLE_LCD_INVERT 0
#endif

#define STRIP_HEIGHT 20
static const char* TAG = "protobuf_display";

static bool transfer_done(esp_lcd_panel_io_handle_t io,
    esp_lcd_panel_io_event_data_t* event, void* context)
{
    (void)io;(void)event;
    lv_display_flush_ready((lv_display_t*)context);
    return false;
}
static void flush(lv_display_t* display,const lv_area_t* area,uint8_t* pixels)
{
    esp_lcd_panel_handle_t panel=(esp_lcd_panel_handle_t)lv_display_get_user_data(display);
    const uint32_t count=(uint32_t)lv_area_get_width(area)*(uint32_t)lv_area_get_height(area);
    /* ST7789 expects high-byte first RGB565; ESP32's render buffer is little endian. */
    lv_draw_rgb565_swap(pixels,count);
    const esp_err_t result=esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,pixels);
    if(result!=ESP_OK) {
        ESP_LOGE(TAG,"Panel flush failed: %s",esp_err_to_name(result));
        lv_display_flush_ready(display);
    }
}
void app_main(void)
{
    lv_init();
    lvgl_pb_esp32_configure_tick();
    if(LV_COLOR_FORMAT_DEFAULT!=LV_COLOR_FORMAT_RGB565) {
        ESP_LOGE(TAG,"Generate this ST7789 example's component with --color-depth 16");
        return;
    }
    if(CONFIG_EXAMPLE_LCD_BACKLIGHT>=0) {
        gpio_set_direction(CONFIG_EXAMPLE_LCD_BACKLIGHT,GPIO_MODE_OUTPUT);
        gpio_set_level(CONFIG_EXAMPLE_LCD_BACKLIGHT,0);
    }
    const size_t buffer_bytes=CONFIG_EXAMPLE_LCD_WIDTH*STRIP_HEIGHT*2;
    spi_bus_config_t bus={
        .sclk_io_num=CONFIG_EXAMPLE_LCD_CLOCK,.mosi_io_num=CONFIG_EXAMPLE_LCD_MOSI,
        .miso_io_num=-1,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=(int)buffer_bytes
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_handle_t io=NULL;
    esp_lcd_panel_io_spi_config_t io_config={
        .dc_gpio_num=CONFIG_EXAMPLE_LCD_DC,.cs_gpio_num=CONFIG_EXAMPLE_LCD_CS,
        .pclk_hz=40*1000*1000,.lcd_cmd_bits=8,.lcd_param_bits=8,.spi_mode=0,.trans_queue_depth=10
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&io_config,&io));
    esp_lcd_panel_handle_t panel=NULL;
    esp_lcd_panel_dev_config_t panel_config={
        .reset_gpio_num=CONFIG_EXAMPLE_LCD_RESET,.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,.bits_per_pixel=16
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io,&panel_config,&panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,CONFIG_EXAMPLE_LCD_INVERT));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel,CONFIG_EXAMPLE_LCD_SWAP_XY));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));

    lv_display_t* display=lv_display_create(CONFIG_EXAMPLE_LCD_WIDTH,CONFIG_EXAMPLE_LCD_HEIGHT);
    if(!display){ESP_LOGE(TAG,"Cannot create LVGL display");return;}
    void* first=heap_caps_malloc(buffer_bytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
    void* second=heap_caps_malloc(buffer_bytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
    if(!first || !second){free(first);free(second);lv_display_delete(display);ESP_LOGE(TAG,"Cannot allocate DMA draw buffers");return;}
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,first,second,(uint32_t)buffer_bytes,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(display,panel);
    lv_display_set_flush_cb(display,flush);
    esp_lcd_panel_io_callbacks_t callbacks={.on_color_trans_done=transfer_done};
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io,&callbacks,display));
    lvgl_runtime_t* project=NULL;
    char error[192];
    lvgl_runtime_options_t options={.display=display,.max_input_bytes=4*1024*1024,.load_initial_screen=true};
    const char* project_name=CONFIG_EXAMPLE_PB_PROJECT_NAME[0] ? CONFIG_EXAMPLE_PB_PROJECT_NAME :
        lvgl_pb_embedded_project_count ? lvgl_pb_embedded_projects[0].name : NULL;
    if(!lvgl_pb_esp32_load_embedded(project_name,&options,&project,error,sizeof error))
        ESP_LOGE(TAG,"Protobuf load failed: %s",error);
    if(CONFIG_EXAMPLE_LCD_BACKLIGHT>=0)gpio_set_level(CONFIG_EXAMPLE_LCD_BACKLIGHT,1);
    for(;;) {
        uint32_t delay=lv_timer_handler();
        if(delay<1) delay=1;
        if(delay>20) delay=20;
        TickType_t sleep_ticks=pdMS_TO_TICKS(delay);
        vTaskDelay(sleep_ticks ? sleep_ticks : 1);
    }
    /* If your application shuts down the display, destroy project first and
     * synchronize any queued DMA transfer before freeing its draw buffers. */
}
