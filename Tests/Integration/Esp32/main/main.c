#include "lvgl_pb_esp32.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <assert.h>
#include <stdlib.h>
static void flush(lv_display_t* display,const lv_area_t* area,uint8_t* pixels) {
    (void)area;(void)pixels;lv_display_flush_ready(display);
}
void app_main(void) {
    lv_init();lvgl_pb_esp32_configure_tick();
    lv_display_t* display=lv_display_create(240,320);assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    const size_t bytes=240*20*2;
    void* pixels=heap_caps_malloc(bytes,MALLOC_CAP_8BIT);assert(pixels);
    lv_display_set_buffers(display,pixels,NULL,bytes,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    lvgl_runtime_t* runtime=NULL;char error[256];
    const size_t before=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    assert(lvgl_pb_esp32_load_embedded("hello",NULL,&runtime,error,sizeof error));
    assert(lvgl_runtime_screen_count(runtime)==1);
    lv_obj_t* label=lvgl_runtime_object(runtime,0,0);assert(label);
    assert(lv_obj_check_type(label,&lv_label_class));
    lv_refr_now(display);
    ESP_LOGI("protobuf","Loaded label-only project: heap delta=%u",(unsigned)(before-heap_caps_get_free_size(MALLOC_CAP_8BIT)));
    lvgl_runtime_destroy(runtime);lv_display_delete(display);free(pixels);lv_deinit();
}
