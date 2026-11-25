#include "espos_keyboard/DisplayPort.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace espos::keyboard {
namespace {
constexpr char tag[] = "qemu_keyboard";
constexpr std::int32_t screen_width = 800, screen_height = 480;
constexpr std::uintptr_t registers_address = 0x21000000;
constexpr std::uintptr_t framebuffer_address = 0x20000000;
constexpr std::size_t draw_bytes = screen_width * 40 * sizeof(std::uint16_t);
constexpr uart_port_t pointer_uart = UART_NUM_1;
constexpr std::int64_t touch_timeout_us = 2 * 1000 * 1000;

volatile std::uint32_t& reg(std::size_t offset)
{
    return *reinterpret_cast<volatile std::uint32_t*>(registers_address + offset);
}

struct Touch {
    std::int32_t x = 0, y = 0;
    bool down = false;
};

struct Panel {
    void* draw_buffer = nullptr;
    lv_indev_t* pointer = nullptr;
    lv_timer_t* retry_timer = nullptr;
    lv_display_t* display = nullptr;
    bool uart_open = false;
    bool warned_timeout = false;
    bool needs_redraw = false;
    std::array<Touch, 64> touches{};
    std::size_t head = 0, count = 0;
    Touch current;
    std::int64_t last_touch = 0;
    std::array<char, 40> line{};
    std::size_t line_bytes = 0;
    bool discard_line = false;

    ~Panel()
    {
        if (pointer) lv_indev_delete(pointer);
        if (retry_timer) lv_timer_delete(retry_timer);
        if (uart_open) uart_driver_delete(pointer_uart);
        heap_caps_free(draw_buffer);
    }

    void push(const Touch& touch)
    {
        // Losing a release must never leave a key held. With this many queued
        // events, restart from a release before accepting further input.
        if (count == touches.size()) {
            head = count = 0;
            current.down = false;
            touches[count++] = current;
        }
        touches[(head + count) % touches.size()] = touch;
        ++count;
    }

    void finish_line()
    {
        if (discard_line) { discard_line = false; line_bytes = 0; return; }
        line[line_bytes] = '\0';
        int x = 0, y = 0, down = 0;
        const auto* cursor = line.data();
        const auto* end = cursor + line_bytes;
        const auto number = [&](int& output, bool final) {
            const auto parsed = std::from_chars(cursor, end, output);
            if (parsed.ec != std::errc{} || parsed.ptr == cursor) return false;
            cursor = parsed.ptr;
            if (final) return cursor == end;
            return cursor < end && *cursor++ == ' ';
        };
        const bool command = line_bytes >= 2 && *cursor++ == 'T' && *cursor++ == ' ';
        if (command && number(x, false) && number(y, false) && number(down, true) &&
            x >= 0 && x < screen_width && y >= 0 && y < screen_height && (down == 0 || down == 1)) {
            push({x, y, down == 1});
            last_touch = esp_timer_get_time();
        }
        line_bytes = 0;
    }

    void read_uart()
    {
        std::array<std::uint8_t, 256> bytes{};
        const auto received = uart_read_bytes(pointer_uart, bytes.data(), bytes.size(), 0);
        for (int i = 0; i < received; ++i) {
            const auto value = bytes[static_cast<std::size_t>(i)];
            if (value == '\n') finish_line();
            else if (value != '\r') {
                if (value < 32 || value > 126 || line_bytes + 1 >= line.size()) discard_line = true;
                else if (!discard_line) line[line_bytes++] = static_cast<char>(value);
            }
        }
    }
};

void read_pointer(lv_indev_t* input, lv_indev_data_t* data)
{
    auto& panel = *static_cast<Panel*>(lv_indev_get_user_data(input));
    panel.read_uart();
    if (panel.count) {
        panel.current = panel.touches[panel.head];
        panel.head = (panel.head + 1) % panel.touches.size();
        --panel.count;
    } else if (panel.current.down && esp_timer_get_time() - panel.last_touch > touch_timeout_us) {
        panel.current.down = false;
    }
    data->point.x = panel.current.x;
    data->point.y = panel.current.y;
    data->state = panel.current.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = panel.count != 0;
}

void copy_pixels(lv_display_t* display, const lv_area_t& area, const std::uint8_t* pixels)
{
    auto* framebuffer = reinterpret_cast<std::uint16_t*>(framebuffer_address);
    const auto width = lv_area_get_width(&area), height = lv_area_get_height(&area);
    const auto stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565);
    const auto rotation = lv_display_get_rotation(display);
    for (std::int32_t row = 0; row < height; ++row) {
        const auto* source = reinterpret_cast<const std::uint16_t*>(pixels + row * stride);
        const auto y = area.y1 + row;
        if (rotation == LV_DISPLAY_ROTATION_0 && area.x1 >= 0 && area.x2 < screen_width && y >= 0 && y < screen_height) {
            std::memcpy(framebuffer + y * screen_width + area.x1, source, width * sizeof(*source));
            continue;
        }
        for (std::int32_t column = 0; column < width; ++column) {
            const auto x = area.x1 + column;
            std::int32_t physical_x = x, physical_y = y;
            switch (rotation) {
            case LV_DISPLAY_ROTATION_90: physical_x = y; physical_y = screen_height - x - 1; break;
            case LV_DISPLAY_ROTATION_180: physical_x = screen_width - x - 1; physical_y = screen_height - y - 1; break;
            case LV_DISPLAY_ROTATION_270: physical_x = screen_width - y - 1; physical_y = x; break;
            default: break;
            }
            if (physical_x >= 0 && physical_x < screen_width && physical_y >= 0 && physical_y < screen_height)
                framebuffer[physical_y * screen_width + physical_x] = source[column];
        }
    }
}

void flush(lv_display_t* display, const lv_area_t* area, std::uint8_t* pixels)
{
    auto& panel = *static_cast<Panel*>(lv_display_get_user_data(display));
    copy_pixels(display, *area, pixels);
    if (lv_display_flush_is_last(display)) {
        // QEMU accepts a packed rectangle, not a framebuffer stride. Publish
        // the whole contiguous frame once after LVGL finishes its dirty areas.
        reg(0x08) = 0;
        reg(0x0c) = (screen_width << 16) | screen_height;
        reg(0x10) = static_cast<std::uint32_t>(framebuffer_address);
        reg(0x14) = 1;
        const auto deadline = esp_timer_get_time() + 2 * 1000 * 1000;
        while (reg(0x14) & 1) {
            if (esp_timer_get_time() >= deadline) {
                reg(0x14) = 0;
                if (!panel.warned_timeout) {
                    ESP_LOGW(tag, "Waiting for the QEMU graphics viewer; retrying the frame");
                    panel.warned_timeout = true;
                }
                // LVGL forbids invalidating objects during its render pass.
                // A separate timer retries after this flush has returned.
                panel.needs_redraw = true;
                break;
            }
            vTaskDelay(1);
        }
    }
    lv_display_flush_ready(display);
}

void close_panel(lv_event_t* event)
{
    delete static_cast<Panel*>(lv_event_get_user_data(event));
}

void retry_frame(lv_timer_t* timer)
{
    auto& panel = *static_cast<Panel*>(lv_timer_get_user_data(timer));
    if (panel.needs_redraw) {
        panel.needs_redraw = false;
        lv_obj_invalidate(lv_display_get_screen_active(panel.display));
    }
}
} // namespace

esp_err_t configure_board_display(lv_display_t* display)
{
    if (!display) return ESP_ERR_INVALID_ARG;
    if (reg(0x00) != 0x00000002) return ESP_ERR_NOT_SUPPORTED;
    std::unique_ptr<Panel> panel(new (std::nothrow) Panel);
    if (!panel) return ESP_ERR_NO_MEM;
    panel->draw_buffer = heap_caps_aligned_alloc(64, draw_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!panel->draw_buffer) return ESP_ERR_NO_MEM;
    uart_config_t serial{};
    serial.baud_rate = 115200;
    serial.data_bits = UART_DATA_8_BITS;
    serial.parity = UART_PARITY_DISABLE;
    serial.stop_bits = UART_STOP_BITS_1;
    serial.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    serial.source_clk = UART_SCLK_DEFAULT;
    auto status = uart_param_config(pointer_uart, &serial);
    if (status != ESP_OK) return status;
    status = uart_driver_install(pointer_uart, 2048, 0, 0, nullptr, 0);
    if (status != ESP_OK) return status;
    panel->uart_open = true;
    panel->pointer = lv_indev_create();
    if (!panel->pointer) return ESP_ERR_NO_MEM;
    lv_indev_set_type(panel->pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(panel->pointer, display);
    lv_indev_set_user_data(panel->pointer, panel.get());
    lv_indev_set_read_cb(panel->pointer, read_pointer);
    panel->display = display;
    panel->retry_timer = lv_timer_create(retry_frame, 100, panel.get());
    if (!panel->retry_timer) return ESP_ERR_NO_MEM;
    if (!lv_display_add_event_cb(display, close_panel, LV_EVENT_DELETE, panel.get())) return ESP_ERR_NO_MEM;
    reg(0x14) = 0;
    reg(0x04) = (screen_width << 16) | screen_height;
    reg(0x18) = 16;
    std::memset(reinterpret_cast<void*>(framebuffer_address), 0, screen_width * screen_height * sizeof(std::uint16_t));
    lv_display_set_user_data(display, panel.get());
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, panel->draw_buffer, nullptr, draw_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    panel.release();
    return ESP_OK;
}
} // namespace espos::keyboard
