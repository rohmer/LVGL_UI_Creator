#include "espos_keyboard/Startup.h"

#include <algorithm>
#include <cstdint>

#include "esp_log.h"
#include "esp_timer.h"
#include "espos/TaskScheduler.h"
#include "freertos/FreeRTOS.h"
#include "espos_keyboard/DisplayPort.h"
#include "espos_keyboard/TypingView.h"

namespace espos::keyboard {
namespace {
constexpr char tag[] = "qemu_keyboard";

std::uint32_t tick_ms()
{
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
}

void editor_changed(lv_event_t* event)
{
    auto* editor = static_cast<lv_obj_t*>(lv_event_get_target(event));
    ESP_LOGI(tag, "ESP_QEMU_TEXT:%.*s", 256, lv_textarea_get_text(editor));
}

void keyboard_task(TaskContext& task, void*)
{
    if (task.stop_requested()) return;
    lv_init();
    lv_tick_set_cb(tick_ms);
    auto* display = lv_display_create(800, 480);
    auto status = display ? configure_board_display(display) : ESP_ERR_NO_MEM;
    lv_obj_t* view = nullptr;
    if (status == ESP_OK) {
        view = create_en_us_typing_view(lv_display_get_screen_active(display));
        if (!view) status = ESP_ERR_NO_MEM;
    }
    if (status != ESP_OK) {
        ESP_LOGE(tag, "Could not start the QEMU keyboard: %s", esp_err_to_name(status));
        lv_deinit();
        return;
    }
    auto* editor = typing_view_editor(view);
    if (editor) lv_obj_add_event_cb(editor, editor_changed, LV_EVENT_VALUE_CHANGED, nullptr);
    ESP_LOGI(tag, "ESP_QEMU_KEYBOARD_READY: 800x480 guest display with UART1 touch");
    while (!task.stop_requested()) {
        const auto delay_ms = std::min<std::uint32_t>(lv_timer_handler(), 20);
        if (!task.delay(std::max<TickType_t>(1, pdMS_TO_TICKS(delay_ms)))) break;
    }
    lv_deinit();
}
} // namespace

void start_keyboard()
{
    TaskOptions options;
    options.name = "qemu_keyboard";
    options.stack_bytes = 12288;
    options.priority = tskIDLE_PRIORITY + 2;
    TaskId task_id{};
    const auto result = TaskScheduler::instance().create(options, keyboard_task, nullptr, task_id);
    if (result != ESP_OK) ESP_LOGE(tag, "Could not create the QEMU keyboard task: %s", esp_err_to_name(result));
}
} // namespace espos::keyboard
